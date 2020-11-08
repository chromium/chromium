// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_SODA_INSTALLER_H_
#define CHROME_BROWSER_ACCESSIBILITY_SODA_INSTALLER_H_

#include "base/observer_list.h"

class PrefService;

namespace speech {

// Installer of SODA (Speech On-Device API). This is a singleton because there
// is only one installation of SODA per device.
class SODAInstaller {
 public:
  // Observer of the SODA (Speech On-Device API) installation.
  class Observer : public base::CheckedObserver {
   public:
    // Called when the SODA installation has completed.
    virtual void OnSODAInstalled() = 0;

    // Called if there is an error in the SODA installation.
    virtual void OnSODAError() = 0;

    // Called during the SODA installation. Progress is the download percentage
    // out of 100.
    virtual void OnSODAProgress(int progress) = 0;
  };

  SODAInstaller();
  ~SODAInstaller();
  SODAInstaller(const SODAInstaller&) = delete;
  SODAInstaller& operator=(const SODAInstaller&) = delete;

  // Implemented in the platform-specific subclass to get the SODAInstaller
  // instance.
  static SODAInstaller* GetInstance();

  // Installs the SODA binary. Called by CaptionController when the
  // kLiveCaptionEnabled preference changes. PrefService is passed to share
  // Live Captions preferences: whether it is enabled, which language to
  // download, and what the download filepath should be.
  virtual void InstallSODA(PrefService* prefs) = 0;

  // Installs the user-selected SODA language model. Called by CaptionController
  // when the kLiveCaptionEnabled or kLiveCaptionLanguageCode preferences
  // change. PrefService is passed to share Live Captions preferences: whether
  // it is enabled, which language to download, and what the download filepath
  // should be.
  virtual void InstallLanguage(PrefService* prefs) = 0;

  // Adds an observer to the observer list.
  void AddObserver(Observer* observer);

  // Removes an observer from the observer list.
  void RemoveObserver(Observer* observer);

  // Notifies the observers that the SODA installation has completed.
  void NotifyOnSODAInstalled();

  // Notifies the observers that there is an error in the SODA installation.
  void NotifyOnSODAError();

  // Notifies the observers of the progress percentage as SODA is installed/
  // Progress is the download percentage out of 100.
  void NotifyOnSODAProgress(int progress);

 protected:
  base::ObserverList<Observer> observers_;
};

}  // namespace speech

#endif  // CHROME_BROWSER_ACCESSIBILITY_SODA_INSTALLER_H_
