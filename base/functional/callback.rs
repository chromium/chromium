// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module defines the RustOnceClosure type, which logically equivalent to
//! base::OnceClosure. Later it might contain equivalents for other base
//! callback types.
//!
//! Note that the types in this file are logically equivalent to the C++ types,
//! but they are not FFI compatible. To make a base::OnceClosure from a
//! RustOnceClosure, call base::BindOnce with the closure and the `run`
//! method.

chromium::import! {
    "//base:scoped_refptr";
}

#[cxx::bridge(namespace = "base")]
pub mod ffi {
    // Allow C++ to use RustOnceClosure types
    extern "Rust" {
        #[derive(ExternType)]
        type RustOnceClosure;

        #[Self = "RustOnceClosure"]
        fn run(boxed: Box<RustOnceClosure>);
    }
}

// TODO(crbug.com/493253831): In theory we don't need a box here, we could just
// have this struct be an unsized type since we're going to have to box it
// anyway to call run(). But the cxx bridge complains if we do that, so see if
// we can figure out how to make it work in reality.
pub struct RustOnceClosure(Box<dyn FnOnce()>);

impl RustOnceClosure {
    /// Execute the contained closure.
    ///
    /// Logically, this should take `self` by value, but we want to expose it
    /// in the cxx bridge and that doesn't allow opaque rust types to be taken
    /// by value. So we put it in a Box instead.
    #[allow(clippy::boxed_local)]
    pub fn run(boxed: Box<Self>) {
        boxed.0();
    }
}

impl<T: FnOnce() + 'static> From<T> for RustOnceClosure {
    fn from(f: T) -> Self {
        Self(Box::new(f))
    }
}
