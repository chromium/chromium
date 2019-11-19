// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNC_CLIPBOARD_SERVICE_H_
#define CHROME_BROWSER_SYNC_SYNC_CLIPBOARD_SERVICE_H_

#include <memory>
#include <string>

#include "chrome/browser/image_decoder.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_preferences/pref_service_syncable_observer.h"
#include "components/sync_preferences/synced_pref_observer.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

class SyncClipboardService
    : public KeyedService,
      public sync_preferences::PrefServiceSyncableObserver,
      public sync_preferences::SyncedPrefObserver,
      public ImageDecoder::ImageRequest {
 public:
  SyncClipboardService(
      sync_preferences::PrefServiceSyncable* prefs,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~SyncClipboardService() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  void Start();

  // KeyedService:
  void Shutdown() override;

  // PrefServiceSyncableObserver:
  void OnIsSyncingChanged() override;

  // SyncedPrefObserver:
  void OnSyncedPrefChanged(const std::string& path, bool from_sync) override;

  // ImageRequest:
  void OnImageDecoded(const SkBitmap& decoded_image) override;
  void OnDecodeImageFailed() override;

 private:
  void WriteClipboardText(const std::string& text);
  void FetchImage(const std::string& image_url);
  void OnURLLoadComplete(std::unique_ptr<std::string> content);

  sync_preferences::PrefServiceSyncable* const prefs_;
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
};

#endif  // CHROME_BROWSER_SYNC_SYNC_CLIPBOARD_SERVICE_H_
