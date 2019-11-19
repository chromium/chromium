// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_clipboard_service.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/load_flags.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "url/gurl.h"

namespace {

const char kClipboardTextPrefName[] = "sync_clipboard_service_text";
const char kClipboardImageUrlPrefName[] = "sync_clipboard_service_image_url";

const net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("sync_clipboard_service",
                                        R"(
          semantics {
            sender: "SyncClipboardService"
            description:
              "Fetches an image extracted by the user via an external app, "
              "and puts it on the clipboard."
            trigger: "Image URL received from an app, via Chrome Sync."
            data:
              "An image URL, from a Google storage service like blobstore."
            destination: GOOGLE_OWNED_SERVICE
          }
          policy {
            cookies_allowed: NO
            setting: "Users can disable this behavior by opting out of sync."
            policy_exception_justification: "Can be controlled via sync."
          })");

}  // namespace

SyncClipboardService::SyncClipboardService(
    sync_preferences::PrefServiceSyncable* prefs,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : prefs_(prefs), url_loader_factory_(url_loader_factory) {}

SyncClipboardService::~SyncClipboardService() {}

// static
void SyncClipboardService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  uint32_t flags = user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF;
  registry->RegisterStringPref(kClipboardTextPrefName, "", flags);
  registry->RegisterStringPref(kClipboardImageUrlPrefName, "", flags);
}

void SyncClipboardService::Start() {
  prefs_->AddObserver(this);
  prefs_->AddSyncedPrefObserver(kClipboardTextPrefName, this);
  prefs_->AddSyncedPrefObserver(kClipboardImageUrlPrefName, this);
}

void SyncClipboardService::Shutdown() {
  prefs_->RemoveObserver(this);
  prefs_->RemoveSyncedPrefObserver(kClipboardTextPrefName, this);
  prefs_->RemoveSyncedPrefObserver(kClipboardImageUrlPrefName, this);
}

void SyncClipboardService::OnIsSyncingChanged() {
  if (!prefs_->IsSyncing() || !prefs_->IsPrioritySyncing()) {
    return;
  }

  // Clear any previously set values on startup.
  std::string text = prefs_->GetString(kClipboardTextPrefName);
  if (!text.empty()) {
    prefs_->ClearPref(kClipboardTextPrefName);
  }
  std::string url = prefs_->GetString(kClipboardImageUrlPrefName);
  if (!url.empty()) {
    prefs_->ClearPref(kClipboardImageUrlPrefName);
  }
}

void SyncClipboardService::OnSyncedPrefChanged(const std::string& path,
                                               bool from_sync) {
  if (!from_sync) {
    // Ignore updates that didn't come from sync.
    return;
  }

  if (path == kClipboardTextPrefName) {
    std::string text = prefs_->GetString(kClipboardTextPrefName);
    if (!text.empty()) {
      WriteClipboardText(text);
      prefs_->ClearPref(kClipboardTextPrefName);
    }
  } else if (path == kClipboardImageUrlPrefName) {
    std::string image_url = prefs_->GetString(kClipboardImageUrlPrefName);
    if (!image_url.empty()) {
      FetchImage(image_url);
      prefs_->ClearPref(kClipboardImageUrlPrefName);
    }
  }
}

void SyncClipboardService::WriteClipboardText(const std::string& text) {
  ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
  writer.WriteText(base::UTF8ToUTF16(text));
}

void SyncClipboardService::FetchImage(const std::string& image_url) {
  GURL url(image_url);
  if (!url.is_valid()) {
    return;
  }

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = url;
  request->method = "GET";
  // This request should be unauthenticated (no cookies), and shouldn't be
  // stored in the cache (this URL is only fetched once, ever.)
  request->load_flags = net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  url_loader_ =
      network::SimpleURLLoader::Create(std::move(request), kTrafficAnnotation);
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&SyncClipboardService::OnURLLoadComplete,
                     base::Unretained(this)));
}

void SyncClipboardService::OnURLLoadComplete(
    std::unique_ptr<std::string> content) {
  url_loader_.reset();
  if (!content || content->empty()) {
    return;
  }
  ImageDecoder::Start(this, *content);
}

void SyncClipboardService::OnDecodeImageFailed() {
  DLOG(WARNING) << "Failed to decode image";
}

void SyncClipboardService::OnImageDecoded(const SkBitmap& decoded_image) {
  ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
  writer.WriteImage(decoded_image);
}
