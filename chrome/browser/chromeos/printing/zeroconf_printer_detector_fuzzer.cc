// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuzzer/FuzzedDataProvider.h>

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/chromeos/printing/zeroconf_printer_detector.h"
#include "chrome/browser/local_discovery/service_discovery_device_lister.h"

namespace {

// Describes a single call to ZeroconfDetector.
struct CallToDelegate {
  // method to call
  enum CallType {
    kOnDeviceChanged = 0,
    kOnDeviceRemoved = 1,
    kOnDeviceCacheFlushed = 2,
    kMaxValue = 2
  } call_type;
  // parameters (depends on |call_type|)
  bool added;
  local_discovery::ServiceDescription description;
};

// Shortcut for a map of unique_ptr<ServiceDiscoveryDeviceLister>.
// The key is a name of a service type.
using MapOfListers =
    std::map<std::string,
             std::unique_ptr<local_discovery::ServiceDiscoveryDeviceLister>>;

// Objects of this class fuzzes ZeroconfDetector by calling methods from
// local_discovery::ServiceDiscoveryDeviceLister::Delegate interface.
class FuzzDeviceLister : public local_discovery::ServiceDiscoveryDeviceLister {
 public:
  // |calls| contains definition of calls to made in reverse order.
  FuzzDeviceLister(const std::string& service_type,
                   const std::vector<CallToDelegate>& calls)
      : service_type_(service_type), calls_(calls) {}

  ~FuzzDeviceLister() override = default;

  // Sets a pointer to ZeroconfDetector object and starts fuzzing it.
  void SetDelegate(
      local_discovery::ServiceDiscoveryDeviceLister::Delegate* delegate) {
    delegate_ = delegate;
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&FuzzDeviceLister::CallDelegate,
                                  base::Unretained(this)));
  }

  void Start() override {}
  void DiscoverNewDevices() override {}
  const std::string& service_type() const override { return service_type_; }

 private:
  // Executes single call to
  // local_discovery::ServiceDiscoveryDeviceLister::Delegate interface.
  // This method schedules for execution last call from calls_, removes it from
  // calls_ and at the end schedules itself for execution. It does nothing
  // when calls_ is empty.
  void CallDelegate() {
    if (calls_.empty())
      return;
    const CallToDelegate call = calls_.back();
    calls_.pop_back();
    switch (call.call_type) {
      case CallToDelegate::kOnDeviceChanged:
        base::SequencedTaskRunnerHandle::Get()->PostTask(
            FROM_HERE,
            base::BindOnce(&local_discovery::ServiceDiscoveryDeviceLister::
                               Delegate::OnDeviceChanged,
                           base::Unretained(delegate_), service_type_,
                           call.added, call.description));
        break;
      case CallToDelegate::kOnDeviceRemoved:
        base::SequencedTaskRunnerHandle::Get()->PostTask(
            FROM_HERE,
            base::BindOnce(&local_discovery::ServiceDiscoveryDeviceLister::
                               Delegate::OnDeviceRemoved,
                           base::Unretained(delegate_), service_type_,
                           call.description.service_name));
        break;
      case CallToDelegate::kOnDeviceCacheFlushed:
        base::SequencedTaskRunnerHandle::Get()->PostTask(
            FROM_HERE,
            base::BindOnce(&local_discovery::ServiceDiscoveryDeviceLister::
                               Delegate::OnDeviceCacheFlushed,
                           base::Unretained(delegate_), service_type_));
        break;
    }
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&FuzzDeviceLister::CallDelegate,
                                  base::Unretained(this)));
  }

  local_discovery::ServiceDiscoveryDeviceLister::Delegate* delegate_ = nullptr;
  std::string service_type_;
  std::vector<CallToDelegate> calls_;
};

// Helper for creating a lister.
void CreateLister(const std::string& service_type,
                  const std::vector<CallToDelegate>& calls,
                  MapOfListers* listers) {
  std::unique_ptr<FuzzDeviceLister> fuzz_lister =
      std::make_unique<FuzzDeviceLister>(service_type, std::move(calls));
  listers->emplace(service_type, std::move(fuzz_lister));
}

// Helper for creating a vector of (fuzzing) calls to make.
std::vector<CallToDelegate> CreateFuzzCalls(FuzzedDataProvider* fuzz_data) {
  // local function to generate random int
  auto RandInt = [fuzz_data](int min, int max) -> int {
    return fuzz_data->ConsumeIntegralInRange(min, max);
  };
  // local function to generate random string with random length
  auto RandStr = [fuzz_data](size_t max_length) -> std::string {
    return fuzz_data->ConsumeRandomLengthString(max_length);
  };
  // fuzzing parameters
  constexpr size_t kMaxNumberOfCalls = 100;
  constexpr size_t kMaxNameLength = 1000;
  constexpr size_t kMaxMetadataEntriesCount = 1000;
  constexpr size_t kMaxMetadataEntryLength = 1000;
  // an array of fuzzing calls
  std::vector<CallToDelegate> calls(RandInt(0, kMaxNumberOfCalls));
  // in this array we store names of created services
  std::vector<std::string> names_history;
  names_history.reserve(calls.size() / 2);
  // generates random fuzzing calls
  for (auto& call : calls) {
    call.call_type = static_cast<CallToDelegate::CallType>(
        RandInt(0, CallToDelegate::CallType::kMaxValue));
    switch (call.call_type) {
      case CallToDelegate::kOnDeviceChanged:
        call.description.service_name = RandStr(kMaxNameLength);
        call.description.metadata.resize(RandInt(0, kMaxMetadataEntriesCount));
        for (auto& text : call.description.metadata)
          text = RandStr(kMaxMetadataEntryLength);
        names_history.push_back(call.description.service_name);
        break;
      case CallToDelegate::kOnDeviceRemoved:
        // in 10% of cases random string is used as service name, in the rest
        // 90% one of the name is picked from names_history
        if (names_history.empty() || RandInt(0, 9) == 0) {
          call.description.service_name = RandStr(kMaxNameLength);
        } else {
          const int n = names_history.size();
          call.description.service_name = names_history[RandInt(0, n - 1)];
        }
        break;
      case CallToDelegate::kOnDeviceCacheFlushed:
        break;
    }
  }
  return calls;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  base::test::TaskEnvironment task_environment;
  FuzzedDataProvider fuzz_data(data, size);
  // Creating listers in similar way as in "standard" constructor of
  // ZeroconfPrinterDetector.
  MapOfListers listers;
  std::vector<CallToDelegate> calls = CreateFuzzCalls(&fuzz_data);
  CreateLister(chromeos::ZeroconfPrinterDetector::kIppServiceName, calls,
               &listers);
  CreateLister(chromeos::ZeroconfPrinterDetector::kIppsServiceName, calls,
               &listers);
  CreateLister(chromeos::ZeroconfPrinterDetector::kIppEverywhereServiceName,
               calls, &listers);
  CreateLister(chromeos::ZeroconfPrinterDetector::kIppsEverywhereServiceName,
               calls, &listers);
  // Creating an object of ZeroconfPrinterDetector to fuzz.
  auto detector = chromeos::ZeroconfPrinterDetector::CreateForTesting(&listers);
  for (auto& lf : listers) {
    static_cast<FuzzDeviceLister*>(lf.second.get())
        ->SetDelegate(detector.get());
  }
  // Fuzzing.
  task_environment.RunUntilIdle();
  return 0;
}
