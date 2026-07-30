// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#![deny(unsafe_code)]

use subtle::ConstantTimeEq;

/// Matches C++ `base::UnguessableToken`.
#[repr(C)]
#[allow(clippy::derived_hash_with_manual_eq)]
#[derive(Clone, Copy, Hash, PartialOrd, Ord, Debug, Default)]
pub struct UnguessableToken {
    pub high: u64,
    pub low: u64,
}

impl UnguessableToken {
    pub fn is_empty(&self) -> bool {
        self.high == 0 && self.low == 0
    }
}

impl ConstantTimeEq for UnguessableToken {
    #[inline]
    fn ct_eq(&self, other: &Self) -> subtle::Choice {
        // Constant-time equality comparison to mitigate timing side-channel attacks
        // on bearer tokens, matching C++ base::UnguessableToken operator==.
        self.high.ct_eq(&other.high) & self.low.ct_eq(&other.low)
    }
}

impl PartialEq for UnguessableToken {
    #[inline]
    fn eq(&self, other: &Self) -> bool {
        self.ct_eq(other).into()
    }
}

impl Eq for UnguessableToken {}

// Compile-time checks to ensure memory layout matches C++
// base::UnguessableToken.
const _: () = {
    assert!(std::mem::size_of::<UnguessableToken>() == 16);
    assert!(std::mem::align_of::<UnguessableToken>() == std::mem::align_of::<u64>());
    assert!(std::mem::offset_of!(UnguessableToken, high) == 0);
    assert!(std::mem::offset_of!(UnguessableToken, low) == 8);
};

#[allow(unsafe_code)]
// SAFETY: `base::UnguessableToken` type is trivially copyable and trivially
// destructible in C++, matching the 16-byte layout { uint64_t high; uint64_t
// low; }.
unsafe impl cxx::ExternType for UnguessableToken {
    type Id = cxx::type_id!("base::UnguessableToken");
    type Kind = cxx::kind::Trivial;
}

#[cxx::bridge]
pub mod ffi {
    unsafe extern "C++" {
        include!("base/unguessable_token.h");

        #[namespace = "base"]
        type UnguessableToken = super::UnguessableToken;
    }
}
