// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! **To define a base::Feature in Rust, use the `base_feature` macro.**
//! (Detailed information on usage can be found on the comment for the
//! macro itself, which is defined at the bottom of this file.)

use std::ffi::c_char;
use std::sync::atomic::AtomicU32;

#[doc(hidden)]
pub mod internal {
    /// Secret handshake to (try to) ensure all places that construct a
    /// base::Feature go through the helper `BASE_FEATURE()` macro.
    pub enum FeatureMacroHandshake {
        Secret,
    }
}

/// Specifies whether a given feature is enabled or disabled by default.
///
/// Note: The country-restricted states (`FEATURE_DISABLED_FOR_COUNTRIES` and
// `FEATURE_ENABLED_FOR_COUNTRIES`) defined in C++ are not supported for
/// Rust-defined features and are omitted here.
#[repr(i32)]
#[derive(Clone, Copy)]
pub enum FeatureState {
    Disabled = 0,
    Enabled = 1,
}

/// Rust equivalent of the C++ `base::Feature` type.
///
/// This struct must maintain an identical memory layout to the C++ version
/// for FFI compatibility. Detailed documentation for feature flags can be
/// found in `base/feature.h`.
#[repr(C)]
pub struct Feature {
    /// The name of the feature. Private to enforce null-termination via the
    /// constructor.
    name: &'static c_char,
    /// The default state of the feature.
    pub default_state: FeatureState,
    /// Cached override state, used by C++ FeatureList::IsEnabled.
    /// Initialized to 0.
    cached_value: AtomicU32,
}

// LINT.IfChange(FeatureStruct)
const _: () = {
    assert!(
        std::mem::size_of::<Feature>()
            == if std::mem::size_of::<*const ()>() == 8 { 16 } else { 12 }
    );
    assert!(std::mem::align_of::<Feature>() == std::mem::size_of::<*const ()>());

    assert!(std::mem::offset_of!(Feature, name) == 0);
    assert!(std::mem::offset_of!(Feature, default_state) == std::mem::size_of::<*const ()>());
    assert!(std::mem::offset_of!(Feature, cached_value) == std::mem::size_of::<*const ()>() + 4);
};
// LINT.ThenChange(feature_list_unittest.cc:FeatureStruct)

impl Feature {
    /// Create a new Feature definition where the name is derived from the Rust
    /// identifier. Internal use only via the `base_feature!` macro.
    ///
    /// # Safety
    /// `name` must be a null-terminated string.
    #[doc(hidden)]
    pub const unsafe fn from_id(
        name: &'static str,
        default_state: FeatureState,
        _handshake: internal::FeatureMacroHandshake,
    ) -> Self {
        Self {
            // Safety: name is guaranteed to be null-terminated.
            // We take a pointer here.
            name: unsafe { &*(name.as_ptr() as *const c_char) },
            default_state,
            cached_value: AtomicU32::new(0),
        }
    }

    /// Check if the feature is enabled.
    pub fn is_enabled(&self) -> bool {
        ffi::FeatureList::IsEnabled(self.into())
    }
}

impl<'a> From<&'a Feature> for &'a ffi::Feature {
    fn from(feature: &'a Feature) -> Self {
        // Safety: Feature is ABI-compatible with ffi::Feature (checked by static
        // asserts above).
        unsafe { std::mem::transmute(feature) }
    }
}

// Safety: Feature is intended to be used as a global static and is thread-safe
// thanks to the atomic cached_value.
unsafe impl Sync for Feature {}

/// A parameter for a `base::Feature`.
///
/// Feature parameters allow for tuning experiments from the server side via
/// Finch.
pub struct FeatureParam<T: 'static> {
    pub feature: &'static Feature,
    pub name: &'static str,
    pub default_value: T,
}

impl<T: 'static> FeatureParam<T> {
    /// Creates a new `FeatureParam<T>`.
    pub const fn new(feature: &'static Feature, name: &'static str, default_value: T) -> Self {
        Self { feature, name, default_value }
    }
}

impl FeatureParam<bool> {
    /// Returns the value of the parameter.
    pub fn get(&self) -> bool {
        ffi::get_bool_param(self.feature.into(), self.name, self.default_value)
    }
}

impl FeatureParam<i32> {
    /// Returns the value of the parameter.
    pub fn get(&self) -> i32 {
        ffi::get_int_param(self.feature.into(), self.name, self.default_value)
    }
}

#[cxx::bridge(namespace = "base")]
// Public so other crates can use the Feature type in their own cxx bridges
#[doc(hidden)]
pub mod ffi {
    unsafe extern "C++" {
        include!("base/feature.h");
        include!("base/feature_list.h");
        include!("base/feature_rust_shim.h");
        type Feature;

        #[namespace = "base"]
        type FeatureList;

        #[Self = "FeatureList"]
        fn IsEnabled(feature: &Feature) -> bool;

        #[rust_name = "get_bool_param"]
        fn GetFieldTrialParamByFeatureAsBoolShim(
            feature: &Feature,
            param_name: &str,
            default_value: bool,
        ) -> bool;

        #[rust_name = "get_int_param"]
        fn GetFieldTrialParamByFeatureAsIntShim(
            feature: &Feature,
            param_name: &str,
            default_value: i32,
        ) -> i32;
    }
}

/// The macro for defining base::Features in Rust is `base_feature!`.
///
/// -`$id`is the Rust identifier that will be used for the Feature.
/// - `$default` is the default state to use for the feature. The options are
///   `feature::FeatureState::Disabled` or `feature::FeatureState::Enabled`.
///
/// # Usage:
///
///   use feature::{base_feature, FeatureState};
///
///   base_feature!(MyFeature, FeatureState::Disabled);
///
///   if MyFeature.is_enabled() {
///       // ...
///   }
///
/// Feature names are derived from the `$id` passed to the macro.
/// They should use CamelCase-style naming, e.g. "FooFeature".
///
/// Feature names must be globally unique.
///
/// (Note that features defined in C++ use the `k` prefix as an identifier,
/// meaning kFooFeature is the identifier in C++ code for what is processed
/// by Finch as FooFeature. Rust does not have this `k`-prefix requirement,
/// so a Feature declared in C++ with `kFooFeature` is referenced in Rust as
/// `FooFeature`, and both map to the same underlying Feature.)
#[macro_export]
macro_rules! base_feature {
    // 2-argument version: Derive the name from the identifier.
    ($id:ident, $default:expr) => {
        #[unsafe(no_mangle)]
        #[allow(non_upper_case_globals)]
        pub static $id: $crate::Feature = unsafe {
            // Safety: The string constructed here is explicitly null-terminated.
            $crate::Feature::from_id(
                concat!(stringify!($id), "\0"),
                $default,
                $crate::internal::FeatureMacroHandshake::Secret,
            )
        };
    };
}

/// The macro for defining base::FeatureParams in Rust is `base_feature_param!`.
///
/// - `$id` is the parameter's identifier.
/// - `$name` is the string name of the parameter in Finch configurations.
/// - `$type` is the type of the parameter (`bool` or `i32`).
/// - `$feature` is a reference to the associated `Feature` (e.g. `&MyFeature`).
/// - `$default` is the default value to return when the parameter is not set.
///
/// # Usage:
///
/// ```rust
/// use feature::{base_feature, base_feature_param, FeatureState};
///
/// base_feature!(MyFeature, FeatureState::Disabled);
/// base_feature_param!(MyTimeoutParam, i32, &MyFeature, "timeout_ms", 100);
///
/// if MyFeature.is_enabled() {
///     let timeout = MyTimeoutParam.get();
/// }
/// ```
#[macro_export]
macro_rules! base_feature_param {
    ($id:ident, $type:ty, $feature:expr, $name:expr, $default:expr) => {
        #[allow(non_upper_case_globals)]
        pub static $id: $crate::FeatureParam<$type> =
            $crate::FeatureParam::new($feature, $name, $default);
    };
}
