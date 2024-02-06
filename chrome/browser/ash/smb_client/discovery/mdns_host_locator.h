// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SMB_CLIENT_DISCOVERY_MDNS_HOST_LOCATOR_H_
#define CHROME_BROWSER_ASH_SMB_CLIENT_DISCOVERY_MDNS_HOST_LOCATOR_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/smb_client/discovery/host_locator.h"

namespace ash::smb_client {

// Removes .local from |raw_hostname| if located at the end of the string and
// returns the new hostname.
Hostname RemoveLocal(const std::string& raw_hostname);

// HostLocator implementation that uses mDns to locate hosts.
class MDnsHostLocator : public HostLocator {
 public:
  MDnsHostLocator();

  MDnsHostLocator(const MDnsHostLocator&) = delete;
  MDnsHostLocator& operator=(const MDnsHostLocator&) = delete;

  ~MDnsHostLocator() override;

  // HostLocator override.
  void FindHosts(FindHostsCallback callback) override;

 private:
  // Implementation class, that needs to live on the IO thread.
  class Impl;

  // Helper to post OnFindHostsDone() to the FindHosts() caller thread.
  static void PostFindHostsDone(scoped_refptr<base::TaskRunner> task_runner,
                                FindHostsCallback callback,
                                bool success,
                                const HostMap& hosts);

  // Result callback for Impl::FindHosts().
  void OnFindHostsDone(bool success, const HostMap& hosts);

  // IO thread task runner.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // Implementation that performs the actual mDNS query.
  std::unique_ptr<Impl, base::OnTaskRunnerDeleter> impl_;

  FindHostsCallback callback_;
  SEQUENCE_CHECKER(sequence_checker_);

  // Must be last member.
  base::WeakPtrFactory<MDnsHostLocator> weak_factory_{this};
};

}  // namespace ash::smb_client

#endif  // CHROME_BROWSER_ASH_SMB_CLIENT_DISCOVERY_MDNS_HOST_LOCATOR_H_
