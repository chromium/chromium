// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/component_updater_resource_throttle.h"

#include <vector>

#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/task/post_task.h"
#include "components/component_updater/component_updater_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_throttle.h"

using content::BrowserThread;

namespace component_updater {

namespace {

///////////////////////////////////////////////////////////////////////////////
// In charge of blocking url requests until the |crx_id| component has been
// updated. This class is touched solely from the IO thread. The UI thread
// can post tasks to it via weak pointers. By default the request is blocked
// unless the CrxUpdateService calls Unblock().
// The lifetime is controlled by Chrome's resource loader so the component
// updater cannot touch objects from this class except via weak pointers.
class CUResourceThrottle : public content::ResourceThrottle,
                           public base::SupportsWeakPtr<CUResourceThrottle> {
 public:
  CUResourceThrottle();
  ~CUResourceThrottle() override;

  // Overriden from ResourceThrottle.
  void WillStartRequest(bool* defer) override;
  void WillRedirectRequest(const net::RedirectInfo& redirect_info,
                           bool* defer) override;
  const char* GetNameForLogging() const override;

  // Component updater calls this function via PostTask to unblock the request.
  void Unblock();

  typedef std::vector<base::WeakPtr<CUResourceThrottle> > WeakPtrVector;

 private:
  enum State { NEW, BLOCKED, UNBLOCKED };

  State state_;
};

CUResourceThrottle::CUResourceThrottle() : state_(NEW) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

CUResourceThrottle::~CUResourceThrottle() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void CUResourceThrottle::WillStartRequest(bool* defer) {
  if (state_ != UNBLOCKED) {
    state_ = BLOCKED;
    *defer = true;
  } else {
    *defer = false;
  }
}

void CUResourceThrottle::WillRedirectRequest(
    const net::RedirectInfo& redirect_info, bool* defer) {
  WillStartRequest(defer);
}

const char* CUResourceThrottle::GetNameForLogging() const {
  return "ComponentUpdateResourceThrottle";
}

void CUResourceThrottle::Unblock() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (state_ == BLOCKED)
    Resume();
  state_ = UNBLOCKED;
}

void UnblockThrottleOnUIThread(base::WeakPtr<CUResourceThrottle> rt) {
  base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO})
      ->PostTask(FROM_HERE, base::BindOnce(&CUResourceThrottle::Unblock, rt));
}

}  // namespace

content::ResourceThrottle* GetOnDemandResourceThrottle(
    ComponentUpdateService* cus,
    const std::string& crx_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // We give the raw pointer to the caller, who will delete it at will
  // and we keep for ourselves a weak pointer to it so we can post tasks
  // from the UI thread without having to track lifetime directly.
  CUResourceThrottle* rt = new CUResourceThrottle;
  base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::UI})
      ->PostTask(FROM_HERE,
                 base::BindOnce(&ComponentUpdateService::MaybeThrottle,
                                base::Unretained(cus), crx_id,
                                base::BindOnce(&UnblockThrottleOnUIThread,
                                               rt->AsWeakPtr())));
  return rt;
}

}  // namespace component_updater
