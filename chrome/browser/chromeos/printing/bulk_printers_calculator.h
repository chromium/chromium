// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_BULK_PRINTERS_CALCULATOR_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_BULK_PRINTERS_CALCULATOR_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"

namespace chromeos {

class Printer;

// Calculates a list of available printers from four policies: Data (json with
// all printers), AccessMode (see below), Whitelist and Blacklist (lists with
// ids). All methods must be called from the same sequence and all observers'
// notifications will be called from this sequence. Resultant list of available
// printers are calculated asynchronously on a dedicated internal sequence.
class BulkPrintersCalculator
    : public base::SupportsWeakPtr<BulkPrintersCalculator> {
 public:
  // Algorithm used to calculate a list of available printers from the content
  // of the "Data" policy.
  enum AccessMode {
    UNSET = -1,
    // Printers in the blacklist are disallowed.  Others are allowed.
    BLACKLIST_ONLY = 0,
    // Only printers in the whitelist are allowed.
    WHITELIST_ONLY = 1,
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
  // Sets the "Blacklist" policy. |blacklist| is a list of printers ids.
  virtual void SetBlacklist(const std::vector<std::string>& blacklist) = 0;
  // Sets the "Whitelist" policy. |whitelist| is a list of printers ids.
  virtual void SetWhitelist(const std::vector<std::string>& whitelist) = 0;

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
  // Returns a reference to a resultant list of available printers. Keys are
  // printers ids. If the list of available printers cannot be calculated
  // (because of some error or missing policy), an empty map is returned.
  virtual const std::unordered_map<std::string, Printer>& GetPrinters()
      const = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_BULK_PRINTERS_CALCULATOR_H_
