// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SPARKY_STORAGE_SIMPLE_SIZE_CALCULATOR_H_
#define CHROME_BROWSER_ASH_SPARKY_STORAGE_SIMPLE_SIZE_CALCULATOR_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"

class Profile;

namespace ash {

// Base class for the calculation of a specific storage item. Instances of this
// class rely on their observers calling StartCalculation, and are designed to
// notify observers about the calculated sizes.
class SimpleSizeCalculator {
 public:
  // Enumeration listing the items displayed on the storage page.
  enum class CalculationType {
    kTotal = 0,
    kAvailable,
    kLastCalculationItem = kAvailable,
  };

  // Implement this interface to be notified about item size callbacks.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnSizeCalculated(const CalculationType& item_id,
                                  int64_t total_bytes) = 0;
  };

  // Total number of storage items.
  static constexpr int kCalculationTypeCount =
      static_cast<int>(CalculationType::kLastCalculationItem) + 1;

  explicit SimpleSizeCalculator(const CalculationType& calculation_type);
  virtual ~SimpleSizeCalculator();

  // Starts the size calculation of a given storage item.
  void StartCalculation();

  // Adds an observer.
  virtual void AddObserver(Observer* observer);

  // Removes an observer.
  virtual void RemoveObserver(Observer* observer);

 protected:
  // Performs the size calculation.
  virtual void PerformCalculation() = 0;

  // Notify the StorageHandler about the calculated storage item size
  void NotifySizeCalculated(int64_t total_bytes);

  // Item id.
  const CalculationType calculation_type_;

  // Flag indicating that fetch operations for storage size are ongoing.
  bool calculating_ = false;

  // Observers being notified about storage items size changes.
  base::ObserverList<SimpleSizeCalculator::Observer> observers_;
};

// Class handling the calculation of the total disk space on the system.
class TotalDiskSpaceCalculator : public SimpleSizeCalculator {
 public:
  explicit TotalDiskSpaceCalculator(Profile* profile);

  TotalDiskSpaceCalculator(const TotalDiskSpaceCalculator&) = delete;
  TotalDiskSpaceCalculator& operator=(const TotalDiskSpaceCalculator&) = delete;

  ~TotalDiskSpaceCalculator() override;

 private:
  friend class TotalDiskSpaceTestAPI;

  // SimpleSizeCalculator:
  void PerformCalculation() override;

  void GetRootDeviceSize();

  void OnGetRootDeviceSize(std::optional<int64_t> reply);

  void GetTotalDiskSpace();

  void OnGetTotalDiskSpace(int64_t* total_bytes);

  raw_ptr<Profile> profile_;
  base::WeakPtrFactory<TotalDiskSpaceCalculator> weak_ptr_factory_{this};
};

// Class handling the calculation of the amount free usable disk space.
class FreeDiskSpaceCalculator : public SimpleSizeCalculator {
 public:
  explicit FreeDiskSpaceCalculator(Profile* profile);

  FreeDiskSpaceCalculator(const FreeDiskSpaceCalculator&) = delete;
  FreeDiskSpaceCalculator& operator=(const FreeDiskSpaceCalculator&) = delete;

  ~FreeDiskSpaceCalculator() override;

 private:
  friend class FreeDiskSpaceTestAPI;

  // SimpleSizeCalculator:
  void PerformCalculation() override;

  void GetUserFreeDiskSpace();

  void OnGetUserFreeDiskSpace(std::optional<int64_t> reply);

  void GetFreeDiskSpace();

  void OnGetFreeDiskSpace(int64_t* available_bytes);

  raw_ptr<Profile> profile_;
  base::WeakPtrFactory<FreeDiskSpaceCalculator> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SPARKY_STORAGE_SIMPLE_SIZE_CALCULATOR_H_
