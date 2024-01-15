// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_ONE_GOOGLE_BAR_ONE_GOOGLE_BAR_SERVICE_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_ONE_GOOGLE_BAR_ONE_GOOGLE_BAR_SERVICE_H_

#include <memory>
#include <optional>

#include "base/observer_list.h"
#include "chrome/browser/new_tab_page/one_google_bar/one_google_bar_data.h"
#include "chrome/browser/new_tab_page/one_google_bar/one_google_bar_loader.h"
#include "chrome/browser/new_tab_page/one_google_bar/one_google_bar_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"

namespace signin {
class IdentityManager;
}  // namespace signin

// A service that downloads, caches, and hands out OneGoogleBarData. It never
// initiates a download automatically, only when Refresh is called. When the
// user signs in or out, the cached value is cleared.
class OneGoogleBarService : public KeyedService {
 public:
  OneGoogleBarService(signin::IdentityManager* identity_manager,
                      std::unique_ptr<OneGoogleBarLoader> loader);
  ~OneGoogleBarService() override;

  // KeyedService implementation.
  void Shutdown() override;

  // Returns the currently cached OneGoogleBarData, if any.
  const std::optional<OneGoogleBarData>& one_google_bar_data() const {
    return one_google_bar_data_;
  }

  // Requests an asynchronous refresh from the network. After the update
  // completes, OnOneGoogleBarDataUpdated will be called on the observers.
  void Refresh();

  // Add/remove observers. All observers must unregister themselves before the
  // OneGoogleBarService is destroyed.
  void AddObserver(OneGoogleBarServiceObserver* observer);
  void RemoveObserver(OneGoogleBarServiceObserver* observer);

  OneGoogleBarLoader* loader_for_testing() { return loader_.get(); }

  std::string language_code() { return language_code_; }

  // Used for testing.
  void SetLanguageCodeForTesting(const std::string& language_code);

  // Sets ogdeb query parameter in loader.
  bool SetAdditionalQueryParams(const std::string& value);

 private:
  class SigninObserver;

  void SigninStatusChanged();

  void OneGoogleBarDataLoaded(OneGoogleBarLoader::Status status,
                              const std::optional<OneGoogleBarData>& data);

  void NotifyObservers();

  std::unique_ptr<OneGoogleBarLoader> loader_;

  std::unique_ptr<SigninObserver> signin_observer_;

  base::ObserverList<OneGoogleBarServiceObserver, true>::Unchecked observers_;

  std::optional<OneGoogleBarData> one_google_bar_data_;

  std::string language_code_;
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_ONE_GOOGLE_BAR_ONE_GOOGLE_BAR_SERVICE_H_
