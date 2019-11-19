// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/** @const */ var Constants = {
  /**
   * Key to access wallpaper manifest in chrome.storage.local.
   */
  AccessLocalManifestKey: 'wallpaper-picker-manifest-key',

  /**
   * Key to access user wallpaper info in chrome.storage.local.
   */
  AccessLocalWallpaperInfoKey: 'wallpaper-local-info-key',

  /**
   * Key to access user wallpaper info in chrome.storage.sync.
   */
  AccessSyncWallpaperInfoKey: 'wallpaper-sync-info-key',

  /**
   * Key to access last changed date of a daily refresh wallpaper in
   * chrome.storage.local or chrome.storage.sync.
   */
  AccessLastSurpriseWallpaperChangedDate: 'wallpaper-last-changed-date-key',

  /**
   * Key to access if surprise me feature is enabled or not in
   * chrome.storage.local.
   */
  AccessLocalSurpriseMeEnabledKey: 'surprise-me-enabled-key',

  /**
   * Key to access if surprise me feature is enabled or not in
   * chrome.storage.sync.
   */
  AccessSyncSurpriseMeEnabledKey: 'sync-surprise-me-enabled-key',

  /**
   * Key to access the info related to daily refresh feature in
   * chrome.storage.local.
   */
  AccessLocalDailyRefreshInfoKey: 'daily-refresh-info-key',

  /**
   * Key to access the info related to daily refresh feature in
   * chrome.storage.sync.
   */
  AccessSyncDailyRefreshInfoKey: 'sync-daily-refresh-info-key',

  /**
   * Key to access the images info in chrome.storage.local.
   */
  AccessLocalImagesInfoKey: 'images-info-key',

  /**
   * Key to access the last used language in JSON returned by
   * AccessLocalImagesInfoKey.
   */
  LastUsedLocalImageMappingKey: 'last-used-local-image-mapping',

  /**
   * Key to access the last used language in JSON returned by
   * AccessLocalImagesInfoKey.
   */
  LastUsedLanguageKey: 'last-used-language-key',

  /**
   * Key to access the last used wallpaper image info in chrome.storage.local.
   */
  AccessLastUsedImageInfoKey: 'last-used-image-info-key',

  /**
   * Wallpaper sources enum.
   */
  WallpaperSourceEnum: {
    Online: 'ONLINE',
    Daily: 'DAILY',
    OEM: 'OEM',
    Custom: 'CUSTOM',
    ThirdParty: 'THIRDPARTY',
    Default: 'DEFAULT'
  },

  /**
   * Local storage.
   */
  WallpaperLocalStorage: chrome.storage.local,

  /**
   * Sync storage.
   */
  WallpaperSyncStorage: chrome.storage.sync,

  /**
   * Suffix to append to file name if it is a thumbnail.
   */
  CustomWallpaperThumbnailSuffix: '_thumbnail',

  /**
   * The default layout of each wallpaper thumbnail.
   */
  WallpaperThumbnailDefaultLayout: 'CENTER_CROPPED',

  /**
   * Wallpaper directory enum.
   */
  WallpaperDirNameEnum: {ORIGINAL: 'original', THUMBNAIL: 'thumbnail'},

  /**
   * The filename prefix for a third party wallpaper.
   */
  ThirdPartyWallpaperPrefix: 'third_party_',

  /**
   * The name of the custom event that's fired when the wallpaper is changed by
   * third-party apps.
   */
  WallpaperChangedBy3rdParty: 'wallpaperChangedBy3rdParty'
};
