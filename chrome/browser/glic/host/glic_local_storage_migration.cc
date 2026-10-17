// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_local_storage_migration.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic_client_data_storage.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "components/prefs/pref_service.h"
#include "components/services/storage/public/mojom/local_storage_control.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/dom_storage/storage_area.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace glic {

namespace {

// Background on Blink LocalStorage encoding:
// In `third_party/blink/renderer/modules/storage/cached_storage_area.cc`
// (under `FormatOption::kLocalStorageDetectFormat`), Blink serializes
// LocalStorage keys and values with a 1-byte format prefix:
// - 0x00 (StorageFormat::UTF16): payload is UTF-16 code units (`char16_t`).
// - 0x01 (StorageFormat::Latin1): payload is Latin-1 characters (0x00..0xFF).
//
// We can't use that code directly here, so we reproduce the logic. This is
// temporary migration code, and can be deleted in a short time horizon, which
// at least limits the cost of the duplication.
enum class DomStorageFormat : uint8_t {
  kUTF16 = 0,
  kLatin1 = 1,
};

bool DOMStorageKeyMatchesASCII(base::span<const uint8_t> key_bytes,
                               std::string_view ascii_key) {
  if (key_bytes == base::as_byte_span(ascii_key)) {
    return true;
  }
  if (key_bytes.size() == ascii_key.size() + 1 &&
      key_bytes[0] == static_cast<uint8_t>(DomStorageFormat::kLatin1) &&
      key_bytes.subspan(1u) == base::as_byte_span(ascii_key)) {
    return true;
  }
  return false;
}

std::optional<std::string_view> GetMatchingTargetKey(
    base::span<const uint8_t> key_bytes) {
  constexpr std::string_view kKeys[] = {
      "BARD_EMBED_CHAT_STORAGE_KEY_V2",
      "WEB_EMBEDDED_CHROME_CAPABILITIES_STORAGE",
  };
  for (std::string_view target_key : kKeys) {
    if (DOMStorageKeyMatchesASCII(key_bytes, target_key)) {
      return target_key;
    }
  }
  return std::nullopt;
}

// Previously, when migrating from the glic partition to the main partition's
// LocalStorage (`blink::mojom::StorageArea`), raw `kv->value` bytes could be
// copied directly from one `StorageArea` to another without decoding. Because
// we now migrate into a JSON-backed dictionary pref
// (`prefs::kGlicClientDataStorage`), we must decode the Blink DOMStorage
// format prefix (0x00 UTF-16 or 0x01 Latin-1) and convert the payload into a
// valid UTF-8 `std::string`.
std::optional<std::string> DecodeDOMStorageValue(
    base::span<const uint8_t> value_bytes) {
  if (value_bytes.empty()) {
    return std::string();
  }
  auto [format_byte, payload] = value_bytes.split_at<1u>();
  switch (static_cast<DomStorageFormat>(format_byte[0])) {
    case DomStorageFormat::kUTF16: {
      if (payload.size() % sizeof(char16_t) != 0) {
        return std::nullopt;
      }
      std::u16string u16_str(payload.size() / sizeof(char16_t), u'\0');
      base::as_writable_byte_span(u16_str).copy_from(payload);
      std::string utf8_str;
      if (!base::UTF16ToUTF8(u16_str.data(), u16_str.size(), &utf8_str)) {
        return std::nullopt;
      }
      return utf8_str;
    }
    case DomStorageFormat::kLatin1: {
      std::u16string u16_str(payload.begin(), payload.end());
      return base::UTF16ToUTF8(u16_str);
    }
  }
  return std::nullopt;
}

class GlicLocalStorageMigrator {
 public:
  static void Run(Profile* profile,
                  GlicClientDataStorage* client_data_storage,
                  content::StoragePartition* source_partition,
                  const blink::StorageKey& storage_key) {
    new GlicLocalStorageMigrator(profile, client_data_storage, source_partition,
                                 storage_key);
  }

 private:
  GlicLocalStorageMigrator(Profile* profile,
                           GlicClientDataStorage* client_data_storage,
                           content::StoragePartition* source_partition,
                           const blink::StorageKey& storage_key)
      : profile_(profile->GetWeakPtr()),
        client_data_storage_(client_data_storage->GetWeakPtr()) {
    storage::mojom::LocalStorageControl* source_control =
        source_partition->GetLocalStorageControl();
    if (!source_control) {
      Finish(/*success=*/false);
      return;
    }

    source_control->BindStorageArea(storage_key,
                                    source_area_.BindNewPipeAndPassReceiver());

    source_area_.set_disconnect_handler(
        base::BindOnce(&GlicLocalStorageMigrator::Finish,
                       weak_ptr_factory_.GetWeakPtr(), /*success=*/false));

    source_area_->GetAll(
        /*new_observer=*/mojo::NullRemote(),
        base::BindOnce(&GlicLocalStorageMigrator::OnGetAll,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  ~GlicLocalStorageMigrator() = default;

  void OnGetAll(std::vector<blink::mojom::KeyValuePtr> data) {
    if (!profile_ || !client_data_storage_) {
      Finish(/*success=*/false);
      return;
    }

    for (const auto& kv : data) {
      std::optional<std::string_view> target_key =
          GetMatchingTargetKey(kv->key);
      if (!target_key) {
        continue;
      }
      // Do not overwrite keys that have already been set in storage.
      if (client_data_storage_->Contains(*target_key)) {
        continue;
      }
      if (std::optional<std::string> decoded_value =
              DecodeDOMStorageValue(kv->value)) {
        client_data_storage_->SetItem(*target_key, std::move(*decoded_value));
      }
    }

    // Deletes this.
    Finish(/*success=*/true);
  }

  void Finish(bool success) {
    if (finished_) {
      return;
    }
    finished_ = true;
    if (success && profile_) {
      profile_->GetPrefs()->SetBoolean(prefs::kGlicLocalStorageCopiedToPrefs,
                                       true);
    }
    delete this;
  }

  bool finished_ = false;
  base::WeakPtr<Profile> profile_;
  base::WeakPtr<GlicClientDataStorage> client_data_storage_;
  mojo::Remote<blink::mojom::StorageArea> source_area_;
  base::WeakPtrFactory<GlicLocalStorageMigrator> weak_ptr_factory_{this};
};

}  // namespace

void MaybeMigrateGlicLocalStorage(content::BrowserContext* browser_context,
                                  GlicClientDataStorage* client_data_storage) {
  if (!GlicKeyedService::IsNoWebviewEnabled(browser_context) ||
      !base::FeatureList::IsEnabled(features::kGlicClientDataStorage) ||
      !GlicEnabling::IsEnabledByGlobalCriteria()) {
    return;
  }
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile || profile->IsOffTheRecord() || !client_data_storage ||
      profile->GetPrefs()->GetBoolean(prefs::kGlicLocalStorageCopiedToPrefs)) {
    return;
  }

  content::StoragePartition* source_partition =
      browser_context->GetStoragePartition(
          GetGlicStoragePartitionConfig(browser_context),
          /*can_create=*/false);
  if (!source_partition) {
    // No glic partition, nothing to copy.
    profile->GetPrefs()->SetBoolean(prefs::kGlicLocalStorageCopiedToPrefs,
                                    true);
    return;
  }

  blink::StorageKey storage_key =
      blink::StorageKey::CreateFirstParty(GetGuestOrigin(profile));

  GlicLocalStorageMigrator::Run(profile, client_data_storage, source_partition,
                                storage_key);
}

}  // namespace glic
