// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_SODA_INSTALLER_H_
#define CHROME_BROWSER_ACCESSIBILITY_SODA_INSTALLER_H_

#include "base/files/file_path.h"
#include "base/observer_list.h"

class PrefService;

namespace speech {

// Installer of SODA (Speech On-Device API). This is a singleton because there
// is only one installation of SODA per device.
class SodaInstaller {
 public:
  // Observer of the SODA (Speech On-Device API) installation.
  class Observer : public base::CheckedObserver {
   public:
    // Called when the SODA installation has completed.
    virtual void OnSodaInstalled() = 0;

    // Called if there is an error in the SODA installation.
    virtual void OnSodaError() = 0;

    // Called during the SODA installation. Progress is the download percentage
    // out of 100.
    virtual void OnSodaProgress(int progress) = 0;
  };

  SodaInstaller();
  virtual ~SodaInstaller();
  SodaInstaller(const SodaInstaller&) = delete;
  SodaInstaller& operator=(const SodaInstaller&) = delete;

  // Implemented in the platform-specific subclass to get the SodaInstaller
  // instance.
  static SodaInstaller* GetInstance();

  // Gets the directory path of the installed SODA lib bundle, or an empty path
  // if not installed. Currently Chrome OS only, returns empty path on other
  // platforms.
  virtual base::FilePath GetSodaBinaryPath() const = 0;

  // Gets the directory path of the installed SODA language bundle, or an empty
  // path if not installed. Currently Chrome OS only, returns empty path on
  // other platforms.
  virtual base::FilePath GetLanguagePath() const = 0;

  // Installs the SODA binary. Called by CaptionController when the
  // kLiveCaptionEnabled preference changes. PrefService is passed to share
  // Live Captions preferences: whether it is enabled, which language to
  // download, and what the download filepath should be.
  virtual void InstallSoda(PrefService* prefs) = 0;

  // Installs the user-selected SODA language model. Called by CaptionController
  // when the kLiveCaptionEnabled or kLiveCaptionLanguageCode preferences
  // change. PrefService is passed to share Live Captions preferences: whether
  // it is enabled, which language to download, and what the download filepath
  // should be.
  virtual void InstallLanguage(PrefService* prefs) = 0;

  // Returns whether or not SODA is installed on this device. Will return a
  // stale value until InstallSoda() and InstallLanguage() have run and
  // asynchronously returned an answer.
  virtual bool IsSodaInstalled() const = 0;

  // Uninstalls SODA and associated language model(s). On some platforms, disc
  // space may not be freed immediately.
  virtual void UninstallSoda(PrefService* global_prefs) = 0;

  // Adds an observer to the observer list.
  void AddObserver(Observer* observer);

  // Removes an observer from the observer list.
  void RemoveObserver(Observer* observer);

  void NotifySodaInstalledForTesting();

 protected:
  // Notifies the observers that the SODA installation has completed.
  void NotifyOnSodaInstalled();

  // Notifies the observers that there is an error in the SODA installation.
  void NotifyOnSodaError();

  // Notifies the observers of the progress percentage as SODA is installed/
  // Progress is the download percentage out of 100.
  void NotifyOnSodaProgress(int progress);

  base::ObserverList<Observer> observers_;
  bool soda_binary_installed_ = false;
  bool language_installed_ = false;
};

}  // namespace speech

#endif  // CHROME_BROWSER_ACCESSIBILITY_SODA_INSTALLER_H_
