// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/test_support/fake_shell_delegate.h"

#include <string>
#include <utility>

#include "base/functional/callback.h"

namespace default_browser {

FakeShellDelegate::FakeShellDelegate() = default;

FakeShellDelegate::~FakeShellDelegate() = default;

void FakeShellDelegate::StartCheckIsDefault(
    shell_integration::DefaultWebClientWorkerCallback callback) {
  std::move(callback).Run(default_state_);
}

#if BUILDFLAG(IS_WIN)
void FakeShellDelegate::StartCheckDefaultClientProgId(
    const GURL& scheme,
    base::OnceCallback<void(const std::u16string&)> callback) {
  std::u16string prog_id = u"";
  if (scheme.scheme() == "http") {
    prog_id = http_assoc_prog_id_;
  }
  std::move(callback).Run(prog_id);
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace default_browser
