// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/instant_io_context.h"

#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_context.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace {

// Retrieves the Instant data from the |context|'s named user-data.
InstantIOContext* GetDataForResourceContext(
    content::ResourceContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!context)
    return nullptr;

  return base::UserDataAdapter<InstantIOContext>::Get(
      context, InstantIOContext::kInstantIOContextKeyName);
}

}  // namespace

const char InstantIOContext::kInstantIOContextKeyName[] = "instant_io_context";

InstantIOContext::InstantIOContext() {
  // The InstantIOContext is created on the UI thread but is accessed
  // on the IO thread.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

InstantIOContext::~InstantIOContext() {
}

// static
void InstantIOContext::SetUserDataOnIO(
    content::ResourceContext* resource_context,
    scoped_refptr<InstantIOContext> instant_io_context) {
  resource_context->SetUserData(
      InstantIOContext::kInstantIOContextKeyName,
      std::make_unique<base::UserDataAdapter<InstantIOContext>>(
          instant_io_context.get()));
}

// static
void InstantIOContext::AddInstantProcessOnIO(
    scoped_refptr<InstantIOContext> instant_io_context,
    int process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  instant_io_context->process_ids_.insert(process_id);
}

// static
void InstantIOContext::RemoveInstantProcessOnIO(
    scoped_refptr<InstantIOContext> instant_io_context, int process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  instant_io_context->process_ids_.erase(process_id);
}

// static
void InstantIOContext::ClearInstantProcessesOnIO(
    scoped_refptr<InstantIOContext> instant_io_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  instant_io_context->process_ids_.clear();
}

// static
bool InstantIOContext::ShouldServiceRequest(
    const GURL& url,
    content::ResourceContext* resource_context,
    int render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  InstantIOContext* instant_io_context =
      GetDataForResourceContext(resource_context);
  if (!instant_io_context)
    return false;

  // The process_id for the navigation request will be -1. If
  // so, allow this request since it's not going to another renderer.
  return render_process_id == -1 ||
         instant_io_context->IsInstantProcess(render_process_id);
}

// static
bool InstantIOContext::IsInstantProcess(
    content::ResourceContext* resource_context,
    int render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  InstantIOContext* instant_io_context =
      GetDataForResourceContext(resource_context);
  if (!instant_io_context)
    return false;

  return instant_io_context->IsInstantProcess(render_process_id);
}

bool InstantIOContext::IsInstantProcess(int process_id) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return process_ids_.find(process_id) != process_ids_.end();
}
