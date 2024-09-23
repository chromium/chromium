// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/posix/sysctl.h"

#include <sys/sysctl.h>

#include <initializer_list>
#include <optional>
#include <string>

#include "base/check_op.h"
#include "base/functional/function_ref.h"
#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"

namespace {

std::optional<std::string> StringSysctlImpl(
    base::FunctionRef<int(char* /*out*/, size_t* /*out_len*/)> sysctl_func) {
  size_t buf_len;
  int result = sysctl_func(nullptr, &buf_len);
  if (result < 0 || buf_len < 1) {
    return std::nullopt;
  }

  std::string value(buf_len - 1, '\0');
  result = sysctl_func(&value[0], &buf_len);
  if (result < 0) {
    return std::nullopt;
  }
  CHECK_LE(buf_len - 1, value.size());
  CHECK_EQ(value[buf_len - 1], '\0');
  value.resize(buf_len - 1);

  return value;
}
}  // namespace

namespace base {

std::optional<std::string> StringSysctl(const std::initializer_list<int>& mib) {
  return StringSysctlImpl([mib](char* out, size_t* out_len) {
    return sysctl(const_cast<int*>(std::data(mib)),
                  checked_cast<unsigned int>(std::size(mib)), out, out_len,
                  nullptr, 0);
  });
}

#if !BUILDFLAG(IS_OPENBSD)
std::optional<std::string> StringSysctlByName(const char* name) {
  return StringSysctlImpl([name](char* out, size_t* out_len) {
    return sysctlbyname(name, out, out_len, nullptr, 0);
  });
}
#endif

}  // namespace base
