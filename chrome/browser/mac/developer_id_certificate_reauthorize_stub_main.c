// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "chrome/browser/mac/developer_id_certificate_reauthorize.h"

__attribute__((visibility("default"))) int main(int argc, char* argv[]) {
  exit(DeveloperIDCertificateReauthorizeFromStub(argc,
                                                 (const char* const*)argv));
}
