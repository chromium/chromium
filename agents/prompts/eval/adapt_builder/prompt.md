# Adapt a LUCI builder definition

Before starting the main task, read `//docs/infra/new_builder.md`.

Create a new builder named `win-blink-asan-rel` that builds and runs
web tests on Windows with address sanitization enabled.
The builder should be closely modeled on `linux-blink-asan-rel`.

Builder requirements:
* The builder should have a try version that mirrors its CI counterpart.
* The builder should build and test with standard Windows x86_64 GCE machines.
* In CI, the builder should belong to the `chromium.memory` builder group, with
  `win|blink` as the category in the LUCI console.
* Find suitable existing `.star` files to reuse instead of creating new `.star`
  files.
* Copy the contact email for `linux-blink-asan-rel` to the new builder.

Other instructions:
* All relevant `.star` files to modify are under `//infra/config/`.
* Once the `.star` changes have been written, remember to run
  `lucicfg generate main.star` from `//infra/config/` to generate files for
  LUCI to consume.
* Don't stage or commit any files.
