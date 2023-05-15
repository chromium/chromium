// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

pub fn say_something() -> String {
    #[cfg(feature = "bar_feature")]
    {
        "bar".to_string()
    }
    #[cfg(not(feature = "bar_feature"))]
    {
        "foo".to_string()
    }
}
