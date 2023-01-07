// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/test_blocklist.h"

#include <set>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/extensions/blocklist.h"
#include "chrome/browser/extensions/blocklist_state_fetcher.h"
#include "chrome/browser/extensions/fake_safe_browsing_database_manager.h"

namespace extensions {

namespace {

void Assign(BlocklistState* out, BlocklistState in) {
  *out = in;
}

}  // namespace

BlocklistStateFetcherMock::BlocklistStateFetcherMock() : request_count_(0) {}

BlocklistStateFetcherMock::~BlocklistStateFetcherMock() {}

void BlocklistStateFetcherMock::Request(const std::string& id,
                                        RequestCallback callback) {
  ++request_count_;

  BlocklistState result = NOT_BLOCKLISTED;
  if (base::Contains(states_, id))
    result = states_[id];

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

void BlocklistStateFetcherMock::SetState(const std::string& id,
                                         BlocklistState state) {
  states_[id] = state;
}

void BlocklistStateFetcherMock::Clear() {
  states_.clear();
}

TestBlocklist::TestBlocklist()
    : blocklist_(nullptr),
      blocklist_db_(new FakeSafeBrowsingDatabaseManager(true)),
      scoped_blocklist_db_(blocklist_db_) {}

TestBlocklist::TestBlocklist(Blocklist* blocklist)
    : blocklist_(nullptr),
      blocklist_db_(new FakeSafeBrowsingDatabaseManager(true)),
      scoped_blocklist_db_(blocklist_db_) {
  Attach(blocklist);
}

TestBlocklist::~TestBlocklist() {
  Detach();
}

void TestBlocklist::Attach(Blocklist* blocklist) {
  if (blocklist_)
    Detach();

  blocklist_ = blocklist;
  blocklist_->SetBlocklistStateFetcherForTest(&state_fetcher_mock_);
}

void TestBlocklist::Detach() {
  blocklist_->ResetBlocklistStateFetcherForTest();
  blocklist_->ResetDatabaseUpdatedListenerForTest();
}

void TestBlocklist::SetBlocklistState(const std::string& extension_id,
                                      BlocklistState state,
                                      bool notify) {
  state_fetcher_mock_.SetState(extension_id, state);

  switch (state) {
    case NOT_BLOCKLISTED:
      blocklist_db_->RemoveUnsafe(extension_id);
      break;

    case BLOCKLISTED_MALWARE:
    case BLOCKLISTED_SECURITY_VULNERABILITY:
    case BLOCKLISTED_CWS_POLICY_VIOLATION:
    case BLOCKLISTED_POTENTIALLY_UNWANTED:
      blocklist_db_->AddUnsafe(extension_id);
      break;

    default:
      break;
  }

  if (notify)
    blocklist_db_->NotifyUpdate();
}

void TestBlocklist::Clear(bool notify) {
  state_fetcher_mock_.Clear();
  blocklist_db_->ClearUnsafe();
  if (notify)
    blocklist_db_->NotifyUpdate();
}

BlocklistState TestBlocklist::GetBlocklistState(
    const std::string& extension_id) {
  BlocklistState blocklist_state;
  blocklist_->IsBlocklisted(extension_id,
                            base::BindOnce(&Assign, &blocklist_state));
  base::RunLoop().RunUntilIdle();
  return blocklist_state;
}

void TestBlocklist::DisableSafeBrowsing() {
  blocklist_db_->Disable();
}

void TestBlocklist::EnableSafeBrowsing() {
  blocklist_db_->Enable();
}

void TestBlocklist::NotifyUpdate() {
  blocklist_db_->NotifyUpdate();
}

}  // namespace extensions
