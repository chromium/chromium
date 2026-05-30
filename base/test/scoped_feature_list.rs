// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//base:feature";
}

#[cxx::bridge(namespace = "base::test::rust")]
mod ffi {
    unsafe extern "C++" {
        include!("base/feature.h");
        #[namespace = "base"]
        type Feature;

        include!("base/test/scoped_feature_list_rust_shim.h");

        type ScopedFeatureListRs;

        fn CreateScopedFeatureListRs() -> UniquePtr<ScopedFeatureListRs>;
        fn InitFromCommandLine(
            self: Pin<&mut ScopedFeatureListRs>,
            enable_features: &str,
            disable_features: &str,
        );
        fn InitAndEnableFeature(self: Pin<&mut ScopedFeatureListRs>, feature: &Feature);
        fn InitAndDisableFeature(self: Pin<&mut ScopedFeatureListRs>, feature: &Feature);
    }
}

pub struct ScopedFeatureList {
    inner: cxx::UniquePtr<ffi::ScopedFeatureListRs>,
}

impl ScopedFeatureList {
    pub fn new() -> Self {
        Self { inner: ffi::CreateScopedFeatureListRs() }
    }

    pub fn init_from_command_line(&mut self, enable_features: &str, disable_features: &str) {
        self.inner.pin_mut().InitFromCommandLine(enable_features, disable_features);
    }

    pub fn init_and_enable_feature(&mut self, feature: &feature::Feature) {
        unsafe {
            let feature: &ffi::Feature = std::mem::transmute(feature);
            self.inner.pin_mut().InitAndEnableFeature(feature);
        }
    }

    pub fn init_and_disable_feature(&mut self, feature: &feature::Feature) {
        unsafe {
            let feature: &ffi::Feature = std::mem::transmute(feature);
            self.inner.pin_mut().InitAndDisableFeature(feature);
        }
    }
}

impl Default for ScopedFeatureList {
    fn default() -> Self {
        Self::new()
    }
}
