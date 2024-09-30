// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_CONTEXT_BOUND_OBJECT_H_
#define CHROME_BROWSER_AI_AI_CONTEXT_BOUND_OBJECT_H_

#include "base/containers/unique_ptr_adapters.h"
#include "base/supports_user_data.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/render_frame_host.h"

// Base class for storing an object that shall be deleted when the
// document is gone such as `AIAssistant` and `AISummarizer`.

// The `AIContextBoundObject` will be owned by the `AIContextBoundObjectSet`
// which is bound to the `BucketContext`. However, the `deletion_callback`
// should be set to properly remove the `AIContextBoundObject` from
// `AIContextBoundObjectSet` in case the `AIContextBoundObject` is no longer
// needed for the current context (e.g. the IPC connection is closed before the
// `BucketContext` is destroyed).

// The ownership chain of the relevant class is:
// `BucketContext` (via `SupportsUserData` or `DocumentUserData`) --owns-->
// `AIContextBoundObjectSet` --owns-->
// `AIContextBoundObject` (implements some blink::mojom interface) --owns-->
// `mojo::Receiver<blink::mojom::SomeInterface>`
class AIContextBoundObject {
 public:
  // The deletion_callback will remove the object from the
  // AIContextBoundObjectSet. The handler shall be called when the object is no
  // longer required for the document and should be deleted.
  virtual void SetDeletionCallback(base::OnceClosure deletion_callback) = 0;

  virtual ~AIContextBoundObject() = default;
};

#endif  // CHROME_BROWSER_AI_AI_CONTEXT_BOUND_OBJECT_H_
