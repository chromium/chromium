// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_PROCESS_ARC_PROCESS_H_
#define CHROME_BROWSER_ASH_ARC_PROCESS_ARC_PROCESS_H_

#include <stdint.h>

#include <ostream>
#include <string>
#include <vector>

#include "ash/components/arc/mojom/process.mojom-forward.h"
#include "base/process/process_handle.h"

namespace arc {

class ArcProcess {
 public:
  ArcProcess(base::ProcessId nspid,
             base::ProcessId pid,
             const std::string& process_name,
             mojom::ProcessState process_state,
             bool is_focused,
             int64_t last_activity_time);

  ArcProcess(const ArcProcess&) = delete;
  ArcProcess& operator=(const ArcProcess&) = delete;

  ~ArcProcess();

  ArcProcess(ArcProcess&& other);
  ArcProcess& operator=(ArcProcess&& other);

  // Sort by descending importance.
  bool operator<(const ArcProcess& rhs) const;

  base::ProcessId nspid() const { return nspid_; }
  base::ProcessId pid() const { return pid_; }
  const std::string& process_name() const { return process_name_; }
  mojom::ProcessState process_state() const { return process_state_; }
  bool is_focused() const { return is_focused_; }
  int64_t last_activity_time() const { return last_activity_time_; }
  std::vector<std::string>& packages() { return packages_; }
  const std::vector<std::string>& packages() const { return packages_; }

  void set_process_state(mojom::ProcessState process_state) {
    process_state_ = process_state;
  }

  // Returns true if the process is important and should be protected more
  // from OOM kills than other processes.
  // TODO(raging): Check what stock Android does for handling OOM and modify
  // this function as needed (crbug.com/719537).
  bool IsImportant() const;

  // Returns true if it is persistent process and should have a lower
  // oom_score_adj.
  // TODO(raging): Consider removing this function. Having only IsImportant()
  // might be good enough.
  bool IsPersistent() const;

  // Returns true if the process is cached or empty and should have a higher
  // oom_score_adj to be killed earlier.
  bool IsCached() const;

  // Returns true if process is in the background but should have a lower
  // oom_score_adj.
  bool IsBackgroundProtected() const;

 private:
  // Returns true if this is ARC protected process which we don't allow to kill.
  bool IsArcProtected() const;

  // Returns true if this is key GMS Core or related service which we don't
  // allow to kill.
  bool IsGmsCoreProtected() const;

  base::ProcessId nspid_;
  base::ProcessId pid_;
  std::string process_name_;
  mojom::ProcessState process_state_;
  // If the Android app is the focused window.
  bool is_focused_;
  // A monotonic timer recording the last time this process was active.
  // Milliseconds since Android boot. This info is passed from Android
  // ActivityManagerService via IPC.
  int64_t last_activity_time_;
  std::vector<std::string> packages_;

  friend std::ostream& operator<<(std::ostream& out,
                                  const ArcProcess& arc_process);
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_PROCESS_ARC_PROCESS_H_
