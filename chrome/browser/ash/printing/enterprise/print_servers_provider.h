// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_ENTERPRISE_PRINT_SERVERS_PROVIDER_H_
#define CHROME_BROWSER_ASH_PRINTING_ENTERPRISE_PRINT_SERVERS_PROVIDER_H_

#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/printing/print_server.h"

class PrefRegistrySimple;
class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace ash {

// This class observes values of policies related to external print servers
// and calculates resultant list of available print servers. This list is
// propagated to Observers.
// All methods must be called from the same sequence (UI) and all observers'
// notifications will be called from this sequence.
class PrintServersProvider {
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
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);
  static std::unique_ptr<PrintServersProvider> Create();

  PrintServersProvider(const PrintServersProvider&) = delete;
  PrintServersProvider& operator=(const PrintServersProvider&) = delete;

  virtual ~PrintServersProvider() = default;

  // This method sets the allowlist to calculate resultant list of servers.
  virtual void SetAllowlistPref(PrefService* prefs,
                                const std::string& allowlist_pref) = 0;

  // This method also calls directly OnServersChanged(...) from |observer|.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Sets the content from the policy. |data| is a list of all print servers in
  // JSON format.
  virtual void SetData(std::unique_ptr<std::string> data) = 0;
  // Clears the content of the policy.
  virtual void ClearData() = 0;

  // Returns the list of all print servers given from the data provided in
  // SetData(...) and limited by the allowlist.
  virtual std::optional<std::vector<PrintServer>> GetPrintServers() = 0;

  // Get a WeakPtr to the instance.
  virtual base::WeakPtr<PrintServersProvider> AsWeakPtr() = 0;

 protected:
  PrintServersProvider() = default;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_ENTERPRISE_PRINT_SERVERS_PROVIDER_H_
