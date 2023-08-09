# Debugging Slow Builds

Did you know that Ninja writes a log to disk after each build?

To see what kinds of files took the longest for your previous build:

```sh
cd out/Default
# Lives in depot_tools:
post_build_ninja_summary.py
```

Because the build is highly parallelized the `elapsed time` values are usually
not meaningful so the `weighted time` numbers are calculated to approximate
the impact of build steps on wall-clock time.

You can also set `NINJA_SUMMARIZE_BUILD=1` to have this command run
after each `autoninja` invocation. Setting this environment variable also runs
ninja with `-d stats` which causes it to print out internal information such
as StartEdge times, which measures the times to create processes, and it
modifies the `NINJA_STATUS` environment variable to add information such as how
many processes are running at any given time - both are useful for detecting
slow process creation. You can get this last benefit on its own by setting
`NINJA_STATUS=[%r processes, %f/%t @ %o/s : %es ] ` (trailing space is
intentional).

To generate a Chrome trace of your most recent build:

```sh
git clone https://github.com/nico/ninjatracing
ninjatracing/ninjatracing out/Default/.ninja_log > trace.json
# Then open in https://ui.perfetto.dev/
```

If your build is stuck on a long-running build step you can see what it is by
running `tools/buildstate.py`.

## Slow Bot Builds

Our bots run `ninjatracing` and `post_build_ninja_summary.py` as well.

Find the trace at: `postprocess for reclient > gsutil upload ninja_log > ninja_log`:

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
