// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/dial/dial_media_sink_service.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/media/router/discovery/dial/dial_media_sink_service_impl.h"
#include "components/media_router/common/media_source.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace media_router {

DialMediaSinkService::DialMediaSinkService()
    : impl_(nullptr, base::OnTaskRunnerDeleter(nullptr)) {}

DialMediaSinkService::~DialMediaSinkService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void DialMediaSinkService::Initialize(
    const OnSinksDiscoveredCallback& sink_discovery_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!impl_);

  OnSinksDiscoveredCallback sink_discovery_cb_impl = base::BindRepeating(
      &RunSinksDiscoveredCallbackOnSequence,
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindRepeating(&DialMediaSinkService::RunSinksDiscoveredCallback,
                          weak_ptr_factory_.GetWeakPtr(), sink_discovery_cb));

  impl_ = CreateImpl(sink_discovery_cb_impl);

  impl_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&DialMediaSinkServiceImpl::Initialize,
                                base::Unretained(impl_.get())));
}

void DialMediaSinkService::StartDiscovery() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(impl_);

  if (discovery_started_) {
    return;
  }
  discovery_started_ = true;

  impl_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&DialMediaSinkServiceImpl::StartDiscovery,
                                base::Unretained(impl_.get())));
}

void DialMediaSinkService::DiscoverSinksNow() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(impl_);

  impl_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&DialMediaSinkServiceImpl::DiscoverSinksNow,
                                base::Unretained(impl_.get())));
}

std::unique_ptr<DialMediaSinkServiceImpl, base::OnTaskRunnerDeleter>
DialMediaSinkService::CreateImpl(
    const OnSinksDiscoveredCallback& sink_discovery_cb) {
  // Note: The SequencedTaskRunner needs to be IO thread because DialRegistry
  // runs on IO thread.
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      content::GetIOThreadTaskRunner({});
  return std::unique_ptr<DialMediaSinkServiceImpl, base::OnTaskRunnerDeleter>(
      new DialMediaSinkServiceImpl(sink_discovery_cb, task_runner),
      base::OnTaskRunnerDeleter(task_runner));
}

void DialMediaSinkService::RunSinksDiscoveredCallback(
    const OnSinksDiscoveredCallback& sinks_discovered_cb,
    std::vector<MediaSinkInternal> sinks) {
  sinks_discovered_cb.Run(std::move(sinks));
}

}  // namespace media_router
