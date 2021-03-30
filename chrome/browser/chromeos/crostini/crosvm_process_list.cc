// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crosvm_process_list.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace crostini {

namespace {

// Reads a proc stat file whose pid is |pid|;
// inserts into |pid_stat_map| that maps from pid to its proc stat;
// inserts into |ppid_pids| the maps from its parent pid to its pid;
// assigns pid to |vm_concierge_pid| if this process is vm_concierge.
// |slash_proc| is "/proc" for production and is only changed for tests.
void ProcessProcStatFile(pid_t pid,
                         PidStatMap* pid_stat_map,
                         std::unordered_map<pid_t, std::set<pid_t>>* ppid_pids,
                         pid_t* vm_concierge_pid_out,
                         const base::FilePath& slash_proc) {
  base::FilePath file_path =
      slash_proc.Append(base::NumberToString(pid)).Append("stat");
  base::Optional<ash::system::SingleProcStat> stat =
      ash::system::GetSingleProcStat(file_path);
  if (!stat.has_value())
    return;

  pid_stat_map->emplace(pid, stat.value());
  if (stat.value().name == "vm_concierge") {
    if (*vm_concierge_pid_out != -1) {
      LOG(ERROR) << "More than one vm_concierge process found: "
                 << *vm_concierge_pid_out << " and " << pid << ".";
      *vm_concierge_pid_out = -1;
      return;
    }
    *vm_concierge_pid_out = pid;
  }
  auto it = ppid_pids->find(stat.value().ppid);
  if (it == ppid_pids->end()) {
    std::set<pid_t> pids({pid});
    ppid_pids->emplace(stat.value().ppid, std::move(pids));
  } else {
    it->second.emplace(pid);
  }
}

// Recursively insert |pid| and its children according to |ppid_pids| into
// |crosvm_pids|.
void InsertPid(pid_t pid,
               const std::unordered_map<pid_t, std::set<pid_t>>& ppid_pids,
               std::set<pid_t>* crosvm_pids) {
  DCHECK(crosvm_pids);
  crosvm_pids->insert(pid);
  auto it = ppid_pids.find(pid);
  if (it == ppid_pids.end())
    return;
  for (pid_t cpid : it->second) {
    if (crosvm_pids->find(cpid) != crosvm_pids->end())
      continue;
    InsertPid(cpid, ppid_pids, crosvm_pids);
  }
}

}  // namespace

PidStatMap GetCrosvmPidStatMap(const base::FilePath& slash_proc) {
  PidStatMap crosvm_process_map;
  PidStatMap all_process_map;
  pid_t vm_concierge_pid = -1;
  std::unordered_map<pid_t, std::set<pid_t>> ppid_pids;

  base::FileEnumerator slash_proc_file_enum(slash_proc, false /* recursive */,
                                            base::FileEnumerator::DIRECTORIES);
  for (base::FilePath name = slash_proc_file_enum.Next(); !name.empty();
       name = slash_proc_file_enum.Next()) {
    std::string pid_str = name.BaseName().value();
    pid_t pid;
    if (!base::StringToInt(pid_str, &pid)) {
      continue;
    }
    ProcessProcStatFile(pid, &all_process_map, &ppid_pids, &vm_concierge_pid,
                        slash_proc);
  }

  if (vm_concierge_pid == -1 || all_process_map.empty())
    return crosvm_process_map;

  std::set<pid_t> crosvm_pids;
  InsertPid(vm_concierge_pid, ppid_pids, &crosvm_pids);

  for (pid_t pid : crosvm_pids)
    crosvm_process_map.emplace(pid, all_process_map.at(pid));
  return crosvm_process_map;
}

}  // namespace crostini
