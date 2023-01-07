// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SETTINGS_RESET_PROMPT_DEFAULT_SETTINGS_FETCHER_H_
#define CHROME_BROWSER_SAFE_BROWSING_SETTINGS_RESET_PROMPT_DEFAULT_SETTINGS_FETCHER_H_

#include <memory>

#include "base/functional/callback.h"

class BrandcodedDefaultSettings;
class BrandcodeConfigFetcher;

namespace safe_browsing {

// Class that fetches default settings to be used for the settings reset
// prompt. The static |FetchDefaultSettings()| function will create and manage
// the lifetime of |DefaultSettingsFetcher| instances.
class DefaultSettingsFetcher {
 public:
  using SettingsCallback =
      base::OnceCallback<void(std::unique_ptr<BrandcodedDefaultSettings>)>;

  // Fetches default settings and passes the corresponding
  // |BrandcodedDefaultSettings| object to |callback| on the UI thread. This
  // function must be called on the UI thread as well.
  //
  // If fetching of default settings on the network fails, a default-constructed
  // BrandcodedDefaultSettings object will be passed to |callback|.
  static void FetchDefaultSettings(SettingsCallback callback);
  // Allows tests to specify the default settings that were fetched in
  // |fetched_settings|. Passing nullptr as |fetched_settings| corresponds to
  // the case when fetching default settings over the network fails.
  static void FetchDefaultSettingsForTesting(
      SettingsCallback callback,
      std::unique_ptr<BrandcodedDefaultSettings> fetched_settings);

 private:
  // Instances of |DefaultSettingsFetcher| own themselves and will delete
  // themselves once default settings have been fetched and |callback| has been
  // posted on the UI thread.
  //
  // The main reason for this design is that |BrandcodeConfigFetcher| takes a
  // callback and initiates the fetching process in its constructor, and we need
  // to hold on to the instance of the fetcher until settings have been
  // fetched. This design saves us from having to explicitly manage global
  // |BrandcodeConfigFetcher| instances.
  explicit DefaultSettingsFetcher(SettingsCallback callback);
  ~DefaultSettingsFetcher();

  // Starts the process of fetching default settings and will ensure that
  // |PostCallbackAndDeleteSelf| is called once settings have been fetched.
  void Start();
  void OnSettingsFetched();
  // Posts a call to |callback_| on the UI thread, passing to it
  // |default_settings|, and deletes |this|.
  void PostCallbackAndDeleteSelf(
      std::unique_ptr<BrandcodedDefaultSettings> default_settings);

  std::unique_ptr<BrandcodeConfigFetcher> config_fetcher_;
  SettingsCallback callback_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SETTINGS_RESET_PROMPT_DEFAULT_SETTINGS_FETCHER_H_
