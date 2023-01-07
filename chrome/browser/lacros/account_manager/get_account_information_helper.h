// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_ACCOUNT_MANAGER_GET_ACCOUNT_INFORMATION_HELPER_H_
#define CHROME_BROWSER_LACROS_ACCOUNT_MANAGER_GET_ACCOUNT_INFORMATION_HELPER_H_

#include <atomic>
#include <memory>

#include "chrome/browser/lacros/account_manager/account_profile_mapper.h"
#include "ui/gfx/image/image.h"

class IdentityManagerLacros;

// Helper class. When calling `GetAccountInformationHelper::Start()`, for each
// account in `gaia_ids`, returns one GetAccountInformationResult struct in a
// vector, containing information about these accounts fetched from Ash.
//
// For accounts that Ash does not know about, returns a
// GetAccountInformationResult with empty `email`, `full_name`, and
// `account_image`.
class GetAccountInformationHelper {
 public:
  // A subset of the information from AccountInfo. Used when we have no complete
  // account yet in Lacros, but Ash already has this account. Useful e.g. for UI
  // to add account from Ash into Lacros.
  struct GetAccountInformationResult {
   public:
    GetAccountInformationResult();
    GetAccountInformationResult(const GetAccountInformationResult& other);
    ~GetAccountInformationResult();

    std::string gaia;
    std::string email;
    std::string full_name;
    gfx::Image account_image;
  };

  using GetAccountInformationCallback =
      base::OnceCallback<void(std::vector<GetAccountInformationResult>)>;

  GetAccountInformationHelper();
  // Specify the identity_manager to query information about accounts from.
  // Useful for testing.
  GetAccountInformationHelper(
      std::unique_ptr<IdentityManagerLacros> identity_manager_lacros);
  ~GetAccountInformationHelper();

  GetAccountInformationHelper(const GetAccountInformationHelper&) = delete;
  GetAccountInformationHelper& operator=(const GetAccountInformationHelper&) =
      delete;

  // Starts the flow for querying account information. `callback` must not be
  // null.
  void Start(const std::vector<std::string>& gaia_ids,
             GetAccountInformationCallback callback);

 private:
  // Callbacks for receiving information about accounts.
  void OnFullName(const std::string& gaia_id, const std::string& full_name);
  void OnEmail(const std::string& gaia_id, const std::string& full_name);
  void OnAccountImage(const std::string& gaia_id, const gfx::ImageSkia& image);

  // Runs `callback` after assembling all available information.
  void TriggerCallback();

  // Interface to query account information from.
  std::unique_ptr<IdentityManagerLacros> identity_manager_lacros_;
  // Maps from gaia_id to GetAccountInformationResult
  std::map<std::string, GetAccountInformationResult> account_information_;
  // Runs `TriggerCallback()` once all required information is available.
  base::RepeatingClosure maybe_trigger_callback_;
  // Callback for complete results.
  GetAccountInformationCallback callback_;
};

#endif  // CHROME_BROWSER_LACROS_ACCOUNT_MANAGER_GET_ACCOUNT_INFORMATION_HELPER_H_
