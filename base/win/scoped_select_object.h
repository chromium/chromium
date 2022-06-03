// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_SCOPED_SELECT_OBJECT_H_
#define BASE_WIN_SCOPED_SELECT_OBJECT_H_

#include <windows.h>

#include "base/check.h"
#include "base/macros.h"

namespace base {
namespace win {

// Helper class for deselecting object from DC.
class ScopedSelectObject {
 public:
  ScopedSelectObject(HDC hdc, HGDIOBJ object)
      : hdc_(hdc), oldobj_(SelectObject(hdc, object)) {
    DCHECK(hdc_);
    DCHECK(object);
    DCHECK(oldobj_ != NULL && oldobj_ != HGDI_ERROR);
  }

  ScopedSelectObject(const ScopedSelectObject&) = delete;
  ScopedSelectObject& operator=(const ScopedSelectObject&) = delete;

  ~ScopedSelectObject() {
    HGDIOBJ object = SelectObject(hdc_, oldobj_);
    DCHECK((GetObjectType(oldobj_) != OBJ_REGION && object != NULL) ||
           (GetObjectType(oldobj_) == OBJ_REGION && object != HGDI_ERROR));
  }

 private:
  HDC hdc_;
  HGDIOBJ oldobj_;
};

}  // namespace win
}  // namespace base

#endif  // BASE_WIN_SCOPED_SELECT_OBJECT_H_
