# Chrome on Android App Bundles and Dynamic Feature Modules

This directory contains GN templates and code for Chrome-specific
[dynamic feature modules](/docs/android_dynamic_feature_modules.md).
Among others, it offers the following:

* A list of descriptors for all modules packaged into the Chrome bundles in
  [`chrome_feature_modules.gni`](chrome_feature_modules.gni).

* A GN template to instantiate a Chrome-specific module in
  [`chrome_feature_module_tmpl.gni`](chrome_feature_module_tmpl.gni). It wraps
  an [`android_app_bundle_module`](/build/config/android/rules.gni) and
  adds things like multi ABI (e.g. 64 bit browser and 32 bit WebView) and
  auto-generated Java module descriptors (see
  [here](/components/module_installer/readme.md) for more details).

* A GN template to instantiate a Chrome-specific bundle in
  [`chrome_bundle_tmpl.gni`](chrome_bundle_tmpl.gni). It instantiates a
  `chrome_feature_module` for each passed module descriptors as well as an
  [`android_app_bundle`](/build/config/android/rules.gni).

* A subfolder for each module containing module-specific code such as module
  interfaces and providers.
