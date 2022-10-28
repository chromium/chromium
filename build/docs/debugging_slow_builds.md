# Debugging Slow Builds

Did you know that Ninja writes a log to disk after each build?

To see what kinds of files took the longest for your previous build:

```sh
cd out/Default
# Lives in depot_tools:
post_build_ninja_summary.py
```

You can also set `NINJA_SUMMARIZE_BUILD=1` to have this command run
after each `autoninja` invocation (also runs ninja with `-d stats`).

To generate a Chrome trace of your most recent build:

```sh
git clone https://github.com/nico/ninjatracing
ninjatracing/ninjatracing out/Default/.ninja_log > trace.json
# Then open in https://ui.perfetto.dev/
```

## Slow Bot Builds

Our bots run `ninjatracing` and `post_build_ninja_summary.py` as well.

Find the trace at: `postprocess_for_goma > upload_log > ninja_log`:

 * _".ninja_log in table format (full)"_ is for `post_build_ninja_summary.py`.
 * _"trace viewer (sort_by_end)"_ is for `ninjatracing`.

## Advanced(ish) Tips

* Use `gn gen --tracelog trace.json` to create a trace for `gn gen`.
* Many Android templates make use of
  [`md5_check.py`](https://cs.chromium.org/chromium/src/build/android/gyp/util/md5_check.py)
  to optimize incremental builds.
  * Set `PRINT_BUILD_EXPLANATIONS=1` to have these commands log which inputs
    changed.
* If you suspect files are being rebuilt unnecessarily during incremental
  builds:
  * Use `ninja -n -d explain` to figure out why ninja thinks a target is dirty.
  * Ensure actions are taking advantage of ninja's `restat=1` feature by not
    updating timestamps on outputs when their contents do not change.
    * E.g. by using [`build_utils.AtomicOutput()`]

[`build_utils.AtomicOutput()`]: https://source.chromium.org/search?q=symbol:AtomicOutput%20f:build
