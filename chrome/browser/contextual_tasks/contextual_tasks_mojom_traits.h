// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_MOJOM_TRAITS_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_MOJOM_TRAITS_H_

#include "base/uuid.h"
#include "chrome/browser/contextual_tasks/contextual_tasks.mojom-shared.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_types.h"
#include "mojo/public/cpp/base/unguessable_token_mojom_traits.h"
#include "mojo/public/cpp/base/uuid_mojom_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct StructTraits<contextual_tasks::mojom::ContextualTaskIdDataView,
                    contextual_tasks::ContextualTaskId> {
  static const base::Uuid& value(const contextual_tasks::ContextualTaskId& id) {
    return id.value();
  }

  static bool Read(contextual_tasks::mojom::ContextualTaskIdDataView data,
                   contextual_tasks::ContextualTaskId* out) {
    base::Uuid uuid;
    if (!data.ReadValue(&uuid)) {
      return false;
    }
    *out = contextual_tasks::ContextualTaskId(uuid);
    return true;
  }
};

template <>
struct StructTraits<contextual_tasks::mojom::ContextualWindowIdDataView,
                    contextual_tasks::ContextualWindowId> {
  static const base::UnguessableToken& value(
      const contextual_tasks::ContextualWindowId& id) {
    return id.value();
  }

  static bool Read(contextual_tasks::mojom::ContextualWindowIdDataView data,
                   contextual_tasks::ContextualWindowId* out) {
    base::UnguessableToken token;
    if (!data.ReadValue(&token)) {
      return false;
    }
    *out = contextual_tasks::ContextualWindowId(token);
    return true;
  }
};

}  // namespace mojo

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_MOJOM_TRAITS_H_
