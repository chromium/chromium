This document describes the `.build_config.json` files that are used by the
Chromium build system for Android-specific targets like APK, resources,
and more.

[TOC]

# Overview

Instead of using GN's `metadata` system to propagate information between targets,
every Java target writes a `.params.json` and a `.build_config.json` file with
information needed by targets that depend on them.

They are always written to `$target_gen_dir/$target_name.{build_config,params}.json`.

`.params.json` files are written during "gn gen" with values available at that
time, while `.build_config.json` files are written during the build with values
that are derived from dependent `.json` files.

Build scripts, can accept parameter arguments using `@FileArg references`,
which look like:

    --some-param=@FileArg(foo.build_config.json:<key1>:<key2>:..<keyN>)

This placeholder will ensure that `<filename>` is read as a JSON file, then
return the value at `[key1][key2]...[keyN]` for the `--some-param` option.

Be sure to list the `.build_config.json` in the `action`'s `inputs`.

For a concrete example, consider the following GN fragment:

```gn
# From //ui/android/BUILD.gn:
android_resources("ui_java_resources") {
  custom_package = "org.chromium.ui"
  resource_dirs = [ "java/res" ]
  deps = [
    ":ui_strings_grd",
  ]
}
```

This will end up generating:

**`$CHROMIUM_OUTPUT_DIR/gen/ui/android/ui_java_resources.params.json`:**
```json
{
  "chromium_code": true,
  "deps_configs": [
    "gen/ui/android/ui_strings_grd.build_config.json",
    "gen/third_party/android_sdk/android_sdk_java.build_config.json"
  ],
  "gn_target": "//ui/android:ui_java_resources",
  "res_sources_path": "gen/ui/android/ui_java_resources.res.sources",
  "resources_zip": "obj/ui/android/ui_java_resources.resources.zip",
  "rtxt_path": "gen/ui/android/ui_java_resources_R.txt",
  "type": "android_resources"
}
```

**`$CHROMIUM_OUTPUT_DIR/gen/ui/android/ui_java_resources.build_config.json`:**

```json
{
  "dependency_zip_overlays": [],
  "dependency_zips": [
    "obj/ui/android/ui_strings_grd.resources.zip"
  ],
  "extra_package_names": []
}
```
