// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_KERBEROS_DATA_PIPE_UTILS_H_
#define CHROME_BROWSER_ASH_KERBEROS_DATA_PIPE_UTILS_H_

#include <string>

#include "base/files/scoped_file.h"

namespace ash {
namespace data_pipe_utils {

// Writes |data| to the writing end of a pipe and returns the reading end.
base::ScopedFD GetDataReadPipe(const std::string& data);

}  // namespace data_pipe_utils
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_KERBEROS_DATA_PIPE_UTILS_H_
