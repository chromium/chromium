// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This empty.cc is built when raw_ptr_noop_impl.h is used to avoid
// the situation: no object files are linked to build raw_ptr.dll.
// If raw_ptr target's sources has only header files, no object
// files will be generated and we will see the following error:
// "lld_link:error: <root>: undefined symbol: _DllMainCRTStartup."
