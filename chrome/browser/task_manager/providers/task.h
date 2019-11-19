// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_TASK_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_TASK_H_

#include <stdint.h>

#include <string>

#include "base/macros.h"
#include "base/process/kill.h"
#include "base/process/process_handle.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "components/sessions/core/session_id.h"
#include "third_party/blink/public/common/web_cache/web_cache_resource_type_stats.h"
#include "ui/gfx/image/image_skia.h"

class Profile;

namespace task_manager {

class TaskProviderObserver;

// Defines a task that corresponds to a tab, an app, an extension, ... etc. It
// represents one row in the task manager table. Multiple tasks can share the
// same process, in which case they're grouped together in the task manager
// table. See |task_manager::TaskGroup| which represents a process possibly
// shared by multiple tasks.
class Task {
 public:
  // Note that the declaration order here determines the default sort order
  // in the task manager.
  enum Type {
    UNKNOWN = 0,

    /* Singleton processes first that don't belong to a particular tab. */
    BROWSER,   /* The main browser process. */
    GPU,       /* A graphics process. */
    ARC,       /* An ARC process. */
    CROSTINI,  /* A Crostini VM process. */
    PLUGIN_VM, /* A Plugin VM process. */
    ZYGOTE,    /* A Linux zygote process. */
    UTILITY,   /* A browser utility process. */

    /* Per-Tab processes next. */
    RENDERER,  /* A normal WebContents renderer process. */
    EXTENSION, /* An extension or app process. */

    /* Plugin processes last.*/
    GUEST,          /* A browser plugin guest process. */
    PLUGIN,         /* A plugin process. */
    WORKER,         /* A web worker process. */
    NACL,           /* A NativeClient loader or broker process. */
    SANDBOX_HELPER, /* A sandbox helper process. */
    SERVICE_WORKER, /* A service worker running on the renderer process. */
  };

  // Create a task with the given |title| and the given favicon |icon|. This
  // task runs on a process whose handle is |handle|. |rappor_sample| is the
  // name of the sample to be recorded if this task needs to be reported by
  // Rappor. If |process_id| is not supplied, it will be determined by |handle|.
  Task(const base::string16& title,
       const std::string& rappor_sample,
       const gfx::ImageSkia* icon,
       base::ProcessHandle handle,
       base::ProcessId process_id = base::kNullProcessId);
  virtual ~Task();

  // Gets the name of the given |profile| from the ProfileAttributesStorage.
  static base::string16 GetProfileNameFromProfile(Profile* profile);

  // Activates this TaskManager's task by bringing its container to the front
  // (if possible).
  virtual void Activate();

  // Returns if the task should be killable from the Task Manager UI.
  virtual bool IsKillable();

  // Kills this task.
  virtual void Kill();

  // Will be called to let the task refresh itself between refresh cycles.
  // |update_interval| is the time since the last task manager refresh.
  // the |refresh_flags| indicate which resources should be calculated on each
  // refresh.
  virtual void Refresh(const base::TimeDelta& update_interval,
                       int64_t refresh_flags);

  // Modifies the value of process_id(). To mutate the process ID, this Task is
  // temporarily unregistered from |observer|, and then re-registered before
  // returning.
  void UpdateProcessInfo(base::ProcessHandle handle,
                         base::ProcessId process_id,
                         TaskProviderObserver* observer);

  // Will receive this notification through the task manager from
  // |ChromeNetworkDelegate::OnNetworkBytesReceived()|. The task will add to the
  // |cummulative_read_bytes_|.
  void OnNetworkBytesRead(int64_t bytes_read);

  // Will receive this notification through the task manager from
  // |ChromeNetworkDelegate::OnNetworkBytesSent()|. The task will add to the
  // |cummulative_sent_bytes_| in this refresh cycle.
  void OnNetworkBytesSent(int64_t bytes_sent);

  // Returns the task type.
  virtual Type GetType() const = 0;

  // This is the unique ID of the BrowserChildProcessHost/RenderProcessHost. It
  // is not the PID nor the handle of the process.
  // For a task that represents the browser process, the return value is 0. For
  // other tasks that represent renderers and other child processes, the return
  // value is whatever unique IDs of their hosts in the browser process.
  virtual int GetChildProcessUniqueID() const = 0;

  // If the process, in which this task is running, is terminated, this gets the
  // termination status. Currently implemented only for Renderer processes.
  virtual void GetTerminationStatus(base::TerminationStatus* out_status,
                                    int* out_error_code) const;

  // The name of the profile owning this task.
  virtual base::string16 GetProfileName() const;

  // Returns the unique ID of the tab if this task represents a renderer
  // WebContents used for a tab. Returns SessionID::InvalidValue() if this task
  // does not represent a renderer, or a contents of a tab.
  virtual SessionID GetTabId() const;

  // For Tasks that represent a subactivity of some other task (e.g. a plugin
  // embedded in a page), this returns the Task representing the parent
  // activity.
  bool HasParentTask() const;
  virtual const Task* GetParentTask() const;

  // Getting the Sqlite used memory (in bytes). Not all tasks reports Sqlite
  // memory, in this case a default invalid value of -1 will be returned.
  // Check for whether the task reports it or not first.
  bool ReportsSqliteMemory() const;
  virtual int64_t GetSqliteMemoryUsed() const;

  // Getting the allocated and used V8 memory (in bytes). Not all tasks reports
  // V8 memory, in this case a default invalid value of -1 will be returned.
  // Check for whether the task reports it or not first.
  bool ReportsV8Memory() const;
  virtual int64_t GetV8MemoryAllocated() const;
  virtual int64_t GetV8MemoryUsed() const;

  // Checking if the task reports Webkit resource cache statistics and getting
  // them if it does.
  virtual bool ReportsWebCacheStats() const;
  virtual blink::WebCacheResourceTypeStats GetWebCacheStats() const;

  // Returns the keep-alive counter if the Task is an event page, -1 otherwise.
  virtual int GetKeepaliveCount() const;

  // Returns true if the task is running inside a VM.
  virtual bool IsRunningInVM() const;

  int64_t task_id() const { return task_id_; }

  // Returns the instantaneous rate, in bytes per second, of network usage
  // (sent and received), as measured over the last refresh cycle.
  int64_t network_usage_rate() const {
    return network_sent_rate_ + network_read_rate_;
  }

  // Returns the cumulative number of bytes of network use (sent and received)
  // over the tasks lifetime. It is calculated independently of refreshes and
  // is based on the current |cumulative_bytes_read_| and
  // |cumulative_bytes_sent_|.
  int64_t cumulative_network_usage() const {
    return cumulative_bytes_sent_ + cumulative_bytes_read_;
  }

  const base::string16& title() const { return title_; }
  const std::string& rappor_sample_name() const { return rappor_sample_name_; }
  const gfx::ImageSkia& icon() const { return icon_; }
  const base::ProcessHandle& process_handle() const { return process_handle_; }
  const base::ProcessId& process_id() const { return process_id_; }

 protected:
  // If |*result_image| is not already set, fetch the image with id
  // |id| from the resource database and put in |*result_image|.
  // Returns |*result_image|.
  static gfx::ImageSkia* FetchIcon(int id, gfx::ImageSkia** result_image);
  void set_title(const base::string16& new_title) { title_ = new_title; }
  void set_rappor_sample_name(const std::string& sample) {
    rappor_sample_name_ = sample;
  }
  void set_icon(const gfx::ImageSkia& new_icon) { icon_ = new_icon; }

 private:
  // The unique ID of this task.
  const int64_t task_id_;

  // The sum of all bytes that have been uploaded from this task calculated at
  // the last refresh.
  int64_t last_refresh_cumulative_bytes_sent_;

  // The sum of all bytes that have been downloaded from this task calculated
  // at the last refresh.
  int64_t last_refresh_cumulative_bytes_read_;

  // A continuously updating sum of all bytes that have been uploaded from this
  // task. It is assigned to |last_refresh_cumulative_bytes_sent_| at the end
  // of a refresh.
  int64_t cumulative_bytes_sent_;

  // A continuously updating sum of all bytes that have been downloaded from
  // this task. It is assigned to |last_refresh_cumulative_bytes_sent_| at the
  // end of a refresh.
  int64_t cumulative_bytes_read_;

  // The upload rate (in bytes per second) for this task during the latest
  // refresh.
  int64_t network_sent_rate_;

  // The download rate (in bytes per second) for this task during the latest
  // refresh.
  int64_t network_read_rate_;

  // The title of the task.
  base::string16 title_;

  // The name of the sample representing this task when a Rappor sample needs to
  // be recorded for it.
  std::string rappor_sample_name_;

  // The favicon.
  gfx::ImageSkia icon_;

  // The handle of the process on which this task is running.
  base::ProcessHandle process_handle_;

  // The PID of the process on which this task is running.
  base::ProcessId process_id_;

  DISALLOW_COPY_AND_ASSIGN(Task);
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_TASK_H_
