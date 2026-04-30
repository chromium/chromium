// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_NAMED_TRIGGER_H_
#define BASE_TRACE_EVENT_NAMED_TRIGGER_H_

#include <stdint.h>

#include <map>
#include <optional>
#include <string>

#include "base/base_export.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"

namespace base::trace_event {

inline constexpr char kStartupTracingTriggerName[] = "startup";

// Notifies that a manual trigger event has occurred. Returns true if the
// trigger caused a scenario to either begin recording or finalize the trace
// depending on the config, or false if the trigger had no effect. If the
// trigger specified isn't active in the config, this will do nothing.
BASE_EXPORT bool EmitNamedTrigger(
    const std::string& trigger_name,
    std::optional<int32_t> value = std::nullopt,
    std::optional<uint64_t> flow_id = std::nullopt);

class BASE_EXPORT NamedTriggerManager {
 public:
  NamedTriggerManager();
  virtual ~NamedTriggerManager();

  class Observer : public base::CheckedObserver {
   public:
    virtual bool OnNamedTrigger(std::optional<int32_t> value,
                                uint64_t flow_id) = 0;

   protected:
    ~Observer() override = default;
  };

  static NamedTriggerManager* GetInstance();

  virtual bool DoEmitNamedTrigger(const std::string& trigger_name,
                                  std::optional<int32_t> value,
                                  uint64_t flow_id) = 0;

  void AddObserver(const std::string& name, Observer* observer);
  void RemoveObserver(const std::string& name, Observer* observer);
  void ClearObserversForTesting();

 protected:
  bool NotifyObservers(const std::string& trigger_name,
                       std::optional<int32_t> value,
                       uint64_t flow_id);

  // Sets the instance returns by GetInstance() globally to |manager|.
  static void SetInstance(NamedTriggerManager* manager);

 private:
  std::map<std::string, base::ObserverList<Observer>> observers_;
};

}  // namespace base::trace_event

#endif  // BASE_TRACE_EVENT_NAMED_TRIGGER_H_
