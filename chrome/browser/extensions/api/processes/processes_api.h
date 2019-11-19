// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PROCESSES_PROCESSES_API_H__
#define CHROME_BROWSER_EXTENSIONS_API_PROCESSES_PROCESSES_API_H__

#include <vector>

#include "base/macros.h"
#include "chrome/browser/task_manager/task_manager_observer.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extension_function.h"

class ProcessesApiTest;

namespace extensions {

// Observes the Task Manager and routes the notifications as events to the
// extension system.
class ProcessesEventRouter : public task_manager::TaskManagerObserver {
 public:
  explicit ProcessesEventRouter(content::BrowserContext* context);
  ~ProcessesEventRouter() override;

  // Called when an extension process wants to listen to process events.
  void ListenerAdded();

  // Called when an extension process with a listener exits or removes it.
  void ListenerRemoved();

  // task_manager::TaskManagerObserver:
  void OnTaskAdded(task_manager::TaskId id) override;
  void OnTaskToBeRemoved(task_manager::TaskId id) override;
  void OnTasksRefreshed(const task_manager::TaskIdList& task_ids) override {}
  void OnTasksRefreshedWithBackgroundCalculations(
      const task_manager::TaskIdList& task_ids) override;
  void OnTaskUnresponsive(task_manager::TaskId id) override;

 private:
  friend class ::ProcessesApiTest;

  void DispatchEvent(events::HistogramValue histogram_value,
                     const std::string& event_name,
                     std::unique_ptr<base::ListValue> event_args) const;

  // Determines whether there is a registered listener for the specified event.
  // It helps to avoid collecting data if no one is interested in it.
  bool HasEventListeners(const std::string& event_name) const;

  // Returns true if the task with the given |id| should be reported as created
  // or removed. |out_child_process_host_id| will be filled with the valid ID of
  // the process to report in the event.
  bool ShouldReportOnCreatedOrOnExited(task_manager::TaskId id,
                                       int* out_child_process_host_id) const;

  // Updates the requested task manager refresh types flags depending on what
  // events are being listened to by extensions.
  void UpdateRefreshTypesFlagsBasedOnListeners();

  content::BrowserContext* browser_context_;

  // Count of listeners, so we avoid sending updates if no one is interested.
  int listeners_;

  DISALLOW_COPY_AND_ASSIGN(ProcessesEventRouter);
};

////////////////////////////////////////////////////////////////////////////////
// The profile-keyed service that manages the processes extension API.
class ProcessesAPI : public BrowserContextKeyedAPI,
                     public EventRouter::Observer {
 public:
  explicit ProcessesAPI(content::BrowserContext* context);
  ~ProcessesAPI() override;

  // BrowserContextKeyedAPI:
  static BrowserContextKeyedAPIFactory<ProcessesAPI>* GetFactoryInstance();

  // Convenience method to get the ProcessesAPI for a profile.
  static ProcessesAPI* Get(content::BrowserContext* context);

  // KeyedService:
  void Shutdown() override;

  // EventRouter::Observer:
  void OnListenerAdded(const EventListenerInfo& details) override;
  void OnListenerRemoved(const EventListenerInfo& details) override;

  ProcessesEventRouter* processes_event_router();

 private:
  friend class BrowserContextKeyedAPIFactory<ProcessesAPI>;

  // BrowserContextKeyedAPI:
  static const char* service_name() { return "ProcessesAPI"; }
  static const bool kServiceRedirectedInIncognito = true;
  static const bool kServiceIsNULLWhileTesting = true;

  content::BrowserContext* browser_context_;

  // Created lazily on first access.
  std::unique_ptr<ProcessesEventRouter> processes_event_router_;

  DISALLOW_COPY_AND_ASSIGN(ProcessesAPI);
};

////////////////////////////////////////////////////////////////////////////////
// This extension function returns the Process object for the renderer process
// currently in use by the specified Tab.
class ProcessesGetProcessIdForTabFunction : public ExtensionFunction {
 public:
  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("processes.getProcessIdForTab",
                             PROCESSES_GETPROCESSIDFORTAB)

 private:
  ~ProcessesGetProcessIdForTabFunction() override {}
};

////////////////////////////////////////////////////////////////////////////////
// Extension function that allows terminating Chrome subprocesses, by supplying
// the unique ID for the process coming from the ChildProcess ID pool.
// Using unique IDs instead of OS process IDs allows two advantages:
// * guaranteed uniqueness, since OS process IDs can be reused.
// * guards against killing non-Chrome processes.
class ProcessesTerminateFunction : public ExtensionFunction {
 public:
  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("processes.terminate", PROCESSES_TERMINATE)

 private:
  ~ProcessesTerminateFunction() override {}

  // Functions to get the process handle on the IO thread and post it back to
  // the UI thread from processing.
  base::ProcessHandle GetProcessHandleOnIO(int child_process_host_id) const;
  void OnProcessHandleOnUI(base::ProcessHandle handle);

  // Terminates the process with |handle| if it's valid and is allowed to be
  // terminated. Returns the response value of this extension function to be
  // sent.
  ExtensionFunction::ResponseValue TerminateIfAllowed(
      base::ProcessHandle handle);

  // Caches the parameter of this function. To be accessed only on the UI
  // thread.
  int child_process_host_id_ = 0;
};

////////////////////////////////////////////////////////////////////////////////
// Extension function which returns a set of Process objects, containing the
// details corresponding to the process IDs supplied as input.
class ProcessesGetProcessInfoFunction
    : public ExtensionFunction,
      public task_manager::TaskManagerObserver {
 public:
  ProcessesGetProcessInfoFunction();

  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;

  // task_manager::TaskManagerObserver:
  void OnTaskAdded(task_manager::TaskId id) override {}
  void OnTaskToBeRemoved(task_manager::TaskId id) override {}
  void OnTasksRefreshed(const task_manager::TaskIdList& task_ids) override;
  void OnTasksRefreshedWithBackgroundCalculations(
      const task_manager::TaskIdList& task_ids) override;

  DECLARE_EXTENSION_FUNCTION("processes.getProcessInfo",
                             PROCESSES_GETPROCESSINFO)

 private:
  ~ProcessesGetProcessInfoFunction() override;

  // Since we don't report optional process data like CPU usage in the results
  // of this function, the only background calculations we want to watch is
  // memory usage (which will be requested only when |include_memory_| is true).
  // This function will be called by either OnTasksRefreshed() or
  // OnTasksRefreshedWithBackgroundCalculations() depending on whether memory is
  // requested.
  void GatherDataAndRespond(const task_manager::TaskIdList& task_ids);

  std::vector<int> process_host_ids_;
  bool include_memory_ = false;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PROCESSES_PROCESSES_API_H__
