// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_local_storage_migration.h"

#include <algorithm>
#include <optional>
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
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
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

// Checks whether a DOMStorage key matches a target ASCII string.
//
// Background on Blink LocalStorage encoding:
// In `third_party/blink/renderer/modules/storage/cached_storage_area.cc`
// (under `FormatOption::kLocalStorageDetectFormat`), Blink serializes ASCII/
// Latin-1 LocalStorage keys by allocating a vector of `input.length() + 1`
// bytes. It writes a 1-byte header (`format[0] = StorageFormat::Latin1`, which
// is 0x01) at index 0, and copies the ASCII character bytes into `payload`
// (index 1..end).
//
// We can't use that code directly here, so we reproduce the logic. This is
// temporary migration code, and can be deleted in a short time horizon, which
// at least limits the cost of the duplication.
bool DOMStorageKeyMatchesASCII(base::span<const uint8_t> key_bytes,
                               std::string_view ascii_key) {
  if (key_bytes == base::as_byte_span(ascii_key)) {
    return true;
  }
  if (key_bytes.size() == ascii_key.size() + 1 &&
      key_bytes.subspan(1u) == base::as_byte_span(ascii_key)) {
    return true;
  }
  return false;
}

bool IsKeyToCopy(const std::vector<uint8_t>& key_bytes) {
  constexpr std::string_view kKeys[] = {
      "BARD_EMBED_CHAT_STORAGE_KEY_V2",
      "WEB_EMBEDDED_CHROME_CAPABILITIES_STORAGE",
  };
  for (std::string_view target_key : kKeys) {
    if (DOMStorageKeyMatchesASCII(key_bytes, target_key)) {
      return true;
    }
  }
  return false;
}

class GlicLocalStorageMigrator {
 public:
  static void Run(Profile* profile,
                  content::StoragePartition* source_partition,
                  content::StoragePartition* target_partition,
                  const blink::StorageKey& storage_key) {
    new GlicLocalStorageMigrator(profile, source_partition, target_partition,
                                 storage_key);
  }

 private:
  GlicLocalStorageMigrator(Profile* profile,
                           content::StoragePartition* source_partition,
                           content::StoragePartition* target_partition,
                           const blink::StorageKey& storage_key)
      : profile_(profile->GetWeakPtr()) {
    storage::mojom::LocalStorageControl* source_control =
        source_partition->GetLocalStorageControl();
    storage::mojom::LocalStorageControl* target_control =
        target_partition->GetLocalStorageControl();
    if (!source_control || !target_control) {
      Finish(/*success=*/false);
      return;
    }

    source_control->BindStorageArea(storage_key,
                                    source_area_.BindNewPipeAndPassReceiver());
    target_control->BindStorageArea(storage_key,
                                    target_area_.BindNewPipeAndPassReceiver());

    source_area_.set_disconnect_handler(
        base::BindOnce(&GlicLocalStorageMigrator::Finish,
                       weak_ptr_factory_.GetWeakPtr(), /*success=*/false));
    target_area_.set_disconnect_handler(
        base::BindOnce(&GlicLocalStorageMigrator::Finish,
                       weak_ptr_factory_.GetWeakPtr(), /*success=*/false));

    source_area_->GetAll(
        /*new_observer=*/mojo::NullRemote(),
        base::BindOnce(&GlicLocalStorageMigrator::OnGetAll,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  ~GlicLocalStorageMigrator() = default;

  void OnGetAll(std::vector<blink::mojom::KeyValuePtr> data) {
    std::vector<blink::mojom::KeyValuePtr> to_copy;
    for (auto& kv : data) {
      if (IsKeyToCopy(kv->key)) {
        to_copy.push_back(std::move(kv));
      }
    }

    if (to_copy.empty()) {
      // Deletes this.
      Finish(/*success=*/true);
      return;
    }

    pending_puts_ = to_copy.size();
    for (auto& kv : to_copy) {
      target_area_->Put(kv->key, kv->value, std::nullopt, /*source=*/nullptr,
                        base::BindOnce(&GlicLocalStorageMigrator::OnPut,
                                       weak_ptr_factory_.GetWeakPtr()));
    }
  }

  void OnPut(bool success) {
    if (!success) {
      // Deletes this.
      Finish(/*success=*/false);
      return;
    }
    CHECK_GT(pending_puts_, 0u);
    --pending_puts_;
    if (pending_puts_ == 0) {
      // Deletes this.
      Finish(/*success=*/true);
    }
  }

  void Finish(bool success) {
    if (finished_) {
      return;
    }
    finished_ = true;
    if (success && profile_) {
      profile_->GetPrefs()->SetBoolean(
          prefs::kGlicLocalStorageCopiedToMainPartition, true);
    }
    delete this;
  }

  bool finished_ = false;
  base::WeakPtr<Profile> profile_;
  mojo::Remote<blink::mojom::StorageArea> source_area_;
  mojo::Remote<blink::mojom::StorageArea> target_area_;
  std::vector<std::string_view> target_keys_present_;
  size_t pending_puts_ = 0;
  base::WeakPtrFactory<GlicLocalStorageMigrator> weak_ptr_factory_{this};
};

}  // namespace

void MaybeMigrateGlicLocalStorage(content::BrowserContext* browser_context) {
  if (!base::FeatureList::IsEnabled(features::kGlicNoWebview) ||
      !GlicEnabling::IsEnabledByGlobalCriteria()) {
    return;
  }
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile || profile->IsOffTheRecord() ||
      profile->GetPrefs()->GetBoolean(
          prefs::kGlicLocalStorageCopiedToMainPartition)) {
    return;
  }

  content::StoragePartition* source_partition =
      browser_context->GetStoragePartition(
          GetGlicStoragePartitionConfig(browser_context));
  if (!source_partition) {
    // No glic partition, nothing to copy.
    return;
  }

  content::StoragePartition* target_partition =
      browser_context->GetDefaultStoragePartition();

  blink::StorageKey storage_key =
      blink::StorageKey::CreateFirstParty(GetGuestOrigin());

  GlicLocalStorageMigrator::Run(profile, source_partition, target_partition,
                                storage_key);
}

}  // namespace glic
