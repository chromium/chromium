// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_IDENTITY_DEVICE_OAUTH2_TOKEN_STORE_H_
#define CHROME_BROWSER_DEVICE_IDENTITY_DEVICE_OAUTH2_TOKEN_STORE_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
#include "google_apis/gaia/core_account_id.h"

// An interface to be implemented per-platform that represents an
// encrypted storage facility for the device's robot GAIA account.
class DeviceOAuth2TokenStore {
 public:
  // Implemented by the DeviceOAuth2TokenService to be notified of events
  // related to the state of the token storage.
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when the refresh token becomes available, at which point it'll be
    // returned by a call to |GetRefreshToken()|.
    virtual void OnRefreshTokenAvailable() = 0;
  };

  // Invoked by SetAndSaveRefreshToken to indicate whether the operation was
  // successful or not.
  using StatusCallback = base::OnceCallback<void(bool)>;

  // Called when the |Init()| function finishes.
  // The first param, |init_result|, will be true if the store is properly
  // initialized and ready to use.
  // The 2nd param, |validation_required|, will be true if the calling service
  // is expected to perform validation on the token before using it, false if
  // validation was already completed.
  using InitCallback = base::OnceCallback<void(bool /* init_result */,
                                               bool /* validation_required */)>;

  // Called by |PrepareTrustedAccountId()| once it's done.
  // The param, |trusted_account_present| indicates whether the store was able
  // successfully prepare a trusted Account ID.
  using TrustedAccountIdCallback =
      base::RepeatingCallback<void(bool /* trusted_account_present */)>;

  virtual ~DeviceOAuth2TokenStore() {}

  // Initialize this storage object and perform necessary setup to be able to
  // store/load and encrypt/decrypt the relevant data. Calls
  // |Observer::OnInitComplete()| upon completion.
  virtual void Init(InitCallback callback) = 0;

  // Return the current service account ID for this device.
  virtual CoreAccountId GetAccountId() const = 0;

  // Return the current refresh token for the account ID of the device. This may
  // return the empty string if the token isn't yet ready or if there was an
  // error during initialization.
  virtual std::string GetRefreshToken() const = 0;

  // Persist the given refresh token on the device. Overwrites any previous
  // value. Should only be called during initial device setup. Signals
  // completion via the given callback, passing true if the operation succeeded.
  virtual void SetAndSaveRefreshToken(const std::string& refresh_token,
                                      StatusCallback result_callback) = 0;

  // Requests that this store prepare its underlying storage to be able to be
  // queried for a trusted account ID, whatever that means for that platform.
  // See concrete implementation comments for more details. This does not affect
  // or change this objects' state or the stored token, it is meant to prepare
  // the platform for retrieving the values.
  // Invokes |callback| when the operation completes.
  virtual void PrepareTrustedAccountId(TrustedAccountIdCallback callback) = 0;

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Requests that this store persist the current service account's associated
  // email.
  // On ChromeOS, the account email comes from CrosSettings so this should never
  // be called.
  virtual void SetAccountEmail(const std::string& account_email) = 0;
#endif

  void SetObserver(Observer* observer) { observer_ = observer; }
  Observer* observer() { return observer_; }

 private:
  raw_ptr<Observer> observer_ = nullptr;
};

#endif  // CHROME_BROWSER_DEVICE_IDENTITY_DEVICE_OAUTH2_TOKEN_STORE_H_
