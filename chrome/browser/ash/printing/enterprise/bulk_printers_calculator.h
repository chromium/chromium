// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_ENTERPRISE_BULK_PRINTERS_CALCULATOR_H_
#define CHROME_BROWSER_ASH_PRINTING_ENTERPRISE_BULK_PRINTERS_CALCULATOR_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"

namespace chromeos {
class Printer;
}  // namespace chromeos

namespace ash {

// Calculates a list of available printers from four policies: Data (json with
// all printers), AccessMode (see below), Allowlist and Blocklist (lists with
// ids). All methods must be called from the same sequence and all observers'
// notifications will be called from this sequence. Resultant list of available
// printers are calculated asynchronously on a dedicated internal sequence.
class BulkPrintersCalculator {
 public:
  // Algorithm used to calculate a list of available printers from the content
  // of the "Data" policy.
  enum AccessMode {
    UNSET = -1,
    // Printers in the blocklist are disallowed.  Others are allowed.
    BLOCKLIST_ONLY = 0,
    // Only printers in the allowlist are allowed.
    ALLOWLIST_ONLY = 1,
    // All printers in the "Data" policy are allowed.
    ALL_ACCESS = 2
  };

  class Observer {
   public:
    // Observer is notified by this call when the state of the object changes.
    // See the section "Methods returning the state of the object" below to
    // learn about parameters defining the state of the object. |sender| is
    // a pointer to the object calling the notification.
    virtual void OnPrintersChanged(const BulkPrintersCalculator* sender) = 0;
  };

  static std::unique_ptr<BulkPrintersCalculator> Create();
  virtual ~BulkPrintersCalculator() = default;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // ========================= Methods setting values of the four policies

  // Sets the "Data" policy. |data| is a list of all printers in JSON format.
  virtual void SetData(std::unique_ptr<std::string> data) = 0;
  // Clears the "Data" policy.
  virtual void ClearData() = 0;

  // Sets the "AccessMode" policy. See description of the AccessMode enum.
  virtual void SetAccessMode(AccessMode mode) = 0;
  // Sets the "Blocklist" policy. |blocklist| is a list of printers ids.
  virtual void SetBlocklist(const std::vector<std::string>& blocklist) = 0;
  // Sets the "Allowlist" policy. |allowlist| is a list of printers ids.
  virtual void SetAllowlist(const std::vector<std::string>& allowlist) = 0;

  // ========================= Methods returning the state of the object
  // Methods returning the three parameters defining the state of the object.

  // Returns true if the "Data" policy has been set with SetData(...) method
  // (may be not processed yet). Returns false if the "Data" policy has been
  // cleared with ClearData() method or SetData(...) has been never called.
  virtual bool IsDataPolicySet() const = 0;
  // Returns false if current policies were not processed yet. Returns true
  // if there is no on-going calculations and the method below returns the
  // list of available printers that is up-to-date with current policies.
  virtual bool IsComplete() const = 0;
  // Returns the resultant list of available printers. Keys are printers ids. If
  // the list of available printers cannot be calculated (because of some error
  // or missing policy), an empty map is returned.
  virtual std::unordered_map<std::string, chromeos::Printer> GetPrinters()
      const = 0;

  // Get a WeakPtr to the instance.
  virtual base::WeakPtr<BulkPrintersCalculator> AsWeakPtr() = 0;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_ENTERPRISE_BULK_PRINTERS_CALCULATOR_H_
