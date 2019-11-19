// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_handle.h"

#include <algorithm>

#include "base/logging.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

using content::BrowserThread;

namespace prerender {

PrerenderHandle::Observer::Observer() {
}

PrerenderHandle::Observer::~Observer() {
}

PrerenderHandle::~PrerenderHandle() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (prerender_data_) {
    prerender_data_->contents()->RemoveObserver(this);
  }
}

void PrerenderHandle::SetObserver(Observer* observer) {
  observer_ = observer;
}

void PrerenderHandle::OnNavigateAway() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (prerender_data_)
    prerender_data_->OnHandleNavigatedAway(this);
}

void PrerenderHandle::OnCancel() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (prerender_data_)
    prerender_data_->OnHandleCanceled(this);
}

bool PrerenderHandle::IsPrerendering() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return prerender_data_.get() != nullptr &&
      !prerender_data_->contents()->prerendering_has_been_cancelled();
}

bool PrerenderHandle::IsFinishedLoading() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return prerender_data_ && prerender_data_->contents()->has_finished_loading();
}

bool PrerenderHandle::IsAbandoned() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return prerender_data_ && !prerender_data_->abandon_time().is_null();
}

PrerenderContents* PrerenderHandle::contents() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return prerender_data_ ? prerender_data_->contents() : nullptr;
}

PrerenderHandle::PrerenderHandle(
    PrerenderManager::PrerenderData* prerender_data)
    : observer_(nullptr) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (prerender_data) {
    prerender_data_ = prerender_data->AsWeakPtr();
    prerender_data->OnHandleCreated(this);
  }
}

void PrerenderHandle::OnPrerenderStart(PrerenderContents* prerender_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(prerender_data_);
  DCHECK_EQ(prerender_data_->contents(), prerender_contents);
  if (observer_)
    observer_->OnPrerenderStart(this);
}

void PrerenderHandle::OnPrerenderStopLoading(
    PrerenderContents* prerender_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(prerender_data_);
  DCHECK_EQ(prerender_data_->contents(), prerender_contents);
  if (observer_)
    observer_->OnPrerenderStopLoading(this);
}

void PrerenderHandle::OnPrerenderDomContentLoaded(
    PrerenderContents* prerender_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(prerender_data_);
  DCHECK_EQ(prerender_data_->contents(), prerender_contents);
  if (observer_)
    observer_->OnPrerenderDomContentLoaded(this);
}

void PrerenderHandle::OnPrerenderStop(PrerenderContents* prerender_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (observer_)
    observer_->OnPrerenderStop(this);
}

void PrerenderHandle::OnPrerenderNetworkBytesChanged(
    PrerenderContents* prerender_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (observer_)
    observer_->OnPrerenderNetworkBytesChanged(this);
}

bool PrerenderHandle::RepresentingSamePrerenderAs(
    PrerenderHandle* other) const {
  return other && other->prerender_data_ && prerender_data_ &&
         prerender_data_.get() == other->prerender_data_.get();
}

}  // namespace prerender
