// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCES_INTEGRITY_H_
#define CHROME_BROWSER_RESOURCES_INTEGRITY_H_

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/buildflags.h"
#include "crypto/sha2.h"

// Computes a SHA-256 hash of the contents of file at |path| and compares it
// to the specified |expected_signature|. If no errors occur and the signatures
// match, runs |callback| with |true|; otherwise runs it with |false|.
void CheckResourceIntegrity(
    const base::FilePath& path,
    const base::span<const uint8_t, crypto::kSHA256Length> expected_signature,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::OnceCallback<void(bool)> callback);

// Checks the main Chrome .pak files for corruption by calling
// CheckResourceIntegrity(), using hashes generated from the
// GN target //chrome:packed_resources_integrity.
void CheckPakFileIntegrity();

#endif  // CHROME_BROWSER_RESOURCES_INTEGRITY_H_
