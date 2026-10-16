---
name: general/document-provenance-for-non-chromium-licenses
rubric: readability
severity: nit
---

**Rule.** When a source file outside //third_party retains an external
non-Chromium license header (such as code copied, adapted, or ported from an
upstream project like AOSP), add an explanatory top-of-file comment documenting
where the code was taken from and explaining why it retains that license.

**Why.** Chromium repository files are normally expected to use the canonical
three-line Chromium BSD license header. Adding an explicit provenance comment
clarifies license compliance, tracks external code origins, and avoids confusion
for reviewers, presubmit checks, and downstream embedding projects.

## Examples

Bad:

```cpp
/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 ...
 */

#ifndef BASE_ANDROID_LINKER_ASHMEM_H_
#define BASE_ANDROID_LINKER_ASHMEM_H_
```

Good:

```cpp
/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 ...
 */

// This file is partially taken from AOSP and keeps its license.

#ifndef BASE_ANDROID_LINKER_ASHMEM_H_
#define BASE_ANDROID_LINKER_ASHMEM_H_
```
