* Owners: jonathanjlee@google.com
* Description: Define a LUCI builder in Starlark using an existing builder as a
  guide.
* Git-Revision: 377f1db7a8a7b407dacffb2f56bce6474e913237
* Result:
  * CI and try builders added to `.star` files (see sample diffs below).
  * `lucicfg generate main.star` ran successfully.
* Modified files:
  * `infra/config/generated/builder-owners/chrome-sanitizer-builder-owners@google.com.txt`
  * `infra/config/generated/builders/ci/win-blink-asan-rel/gn-args.json`
  * `infra/config/generated/builders/ci/win-blink-asan-rel/properties.json`
  * `infra/config/generated/builders/ci/win-blink-asan-rel/shadow-properties.json`
  * `infra/config/generated/builders/ci/win-blink-asan-rel/targets/chromium.memory.json`
  * `infra/config/generated/builders/gn_args_locations.json`
  * `infra/config/generated/builders/try/win-blink-asan-rel/gn-args.json`
  * `infra/config/generated/builders/try/win-blink-asan-rel/properties.json`
  * `infra/config/generated/builders/try/win-blink-asan-rel/targets/chromium.memory.json`
  * `infra/config/generated/cq-usage/mega_cq_bots.txt`
  * `infra/config/generated/health-specs/health-specs.json`
  * `infra/config/generated/luci/commit-queue.cfg`
  * `infra/config/generated/luci/cr-buildbucket.cfg`
  * `infra/config/generated/luci/luci-milo.cfg`
  * `infra/config/generated/luci/luci-notify.cfg`
  * `infra/config/generated/luci/luci-scheduler.cfg`
  * `infra/config/generated/sheriff-rotations/chromium.txt`
  * `infra/config/subprojects/chromium/ci/chromium.memory.star`
  * `infra/config/subprojects/chromium/try/tryserver.chromium.win.star`

CI builder diff:

```
diff --git a/infra/config/subprojects/chromium/ci/chromium.memory.star b/infra/config/subprojects/chromium/ci/chromium.memory.star
index 044d95fe7d753..39f54f1e51ba5 100644
--- a/infra/config/subprojects/chromium/ci/chromium.memory.star
+++ b/infra/config/subprojects/chromium/ci/chromium.memory.star
@@ -1135,6 +1135,83 @@ ci.builder(
     ),
 )

+ci.builder(
+    name = "win-blink-asan-rel",
+    description_html = "Runs {} with address-sanitized binaries.".format(
+        linkify(
+            _WEB_TESTS_LINK,
+            "web (platform) tests",
+        ),
+    ),
+    builder_spec = builder_config.builder_spec(
+        gclient_config = builder_config.gclient_config(
+            config = "chromium",
+        ),
+        chromium_config = builder_config.chromium_config(
+            config = "chromium_win_clang_asan",
+            apply_configs = [
+                "mb",
+            ],
+            build_config = builder_config.build_config.RELEASE,
+            target_bits = 64,
+            target_platform = builder_config.target_platform.WIN,
+        ),
+        build_gs_bucket = "chromium-memory-archive",
+    ),
+    gn_args = gn_args.config(
+        configs = [
+            "asan",
+            "release_builder_blink",
+            "remoteexec",
+            "win",
+            "x64",
+        ],
+    ),
+    targets = targets.bundle(
+        targets = [
+            "chromium_webkit_isolated_scripts",
+        ],
+        mixins = [
+            "win10",
+        ],
+        per_test_modifications = {
+            "chrome_wpt_tests": targets.mixin(
+                args = [
+                    "-j6",
+                ],
+            ),
+            "blink_web_tests": targets.mixin(
+                args = [
+                    "--timeout-ms",
+                    "48000",
+                ],
+                swarming = targets.swarming(
+                    shards = 8,
+                ),
+            ),
+            "blink_wpt_tests": targets.mixin(
+                args = [
+                    "--timeout-ms",
+                    "48000",
+                ],
+                swarming = targets.swarming(
+                    shards = 12,
+                ),
+            ),
+            "headless_shell_wpt_tests": targets.mixin(
+                args = [
+                    "-j6",
+                ],
+            ),
+        },
+    ),
+    os = os.WINDOWS_DEFAULT,
+    console_view_entry = consoles.console_view_entry(
+        category = "win|blink",
+        short_name = "asn",
+    ),
+)
+
 ci.builder(
     name = "linux-blink-leak-rel",
     description_html = "Runs {} with {} enabled.".format(
```

Try builder diff:

```
diff --git a/infra/config/subprojects/chromium/try/tryserver.chromium.win.star b/infra/config/subprojects/chromium/try/tryserver.chromium.win.star
index 126f785d47191..109db4381352d 100644
--- a/infra/config/subprojects/chromium/try/tryserver.chromium.win.star
+++ b/infra/config/subprojects/chromium/try/tryserver.chromium.win.star
@@ -112,6 +112,15 @@ try_.builder(
     contact_team_email = "chrome-desktop-engprod@google.com",
 )

+try_.builder(
+    name = "win-blink-asan-rel",
+    mirrors = [
+        "ci/win-blink-asan-rel",
+    ],
+    gn_args = "ci/win-blink-asan-rel",
+    contact_team_email = "chrome-sanitizer-builder-owners@google.com",
+)
+
 try_.builder(
     name = "win-libfuzzer-asan-rel",
     branch_selector = branches.selector.WINDOWS_BRANCHES,
```
