// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_SERVERS_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_SERVERS_PROVIDER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/printing/print_server.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace chromeos {

// This class observes values of policies related to external print servers
// and calculates resultant list of available print servers. This list is
// propagated to Observers.
// All methods must be called from the same sequence (UI) and all observers'
// notifications will be called from this sequence.
class PrintServersProvider
    : public base::SupportsWeakPtr<PrintServersProvider> {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;
    // |complete| is true if all policies have been parsed and applied (even
    // when parsing errors occurred), false means that a new list of available
    // print servers is being calculated or that no calls to SetData(...) or
    // ClearData(...) were made. |servers| contains the current list of
    // available print servers; every server has valid and unique URL. This
    // notification is called when value of any of these two parameters changes.
    virtual void OnServersChanged(bool complete,
                                  const std::vector<PrintServer>& servers) = 0;
  };

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);
  static std::unique_ptr<PrintServersProvider> Create();
  virtual ~PrintServersProvider() = default;

  // This method set profile to fetch non-external policies. It is needed to
  // calculate resultant list of servers.
  virtual void SetProfile(Profile* profile) = 0;

  // This method also calls directly OnServersChanged(...) from |observer|.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Sets the content from the policy. |data| is a list of all print servers in
  // JSON format.
  virtual void SetData(std::unique_ptr<std::string> data) = 0;
  // Clears the content of the policy.
  virtual void ClearData() = 0;

 protected:
  PrintServersProvider() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(PrintServersProvider);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_SERVERS_PROVIDER_H_
