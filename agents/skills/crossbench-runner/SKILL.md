---
name: crossbench-runner
description: >-
  Assists developers in configuring, executing, and analyzing Crossbench
  benchmarks (native commands and web-tests) in Chromium.
trigger: >-
  Load this when the user asks how to run Crossbench, browser_startup,
  web-tests, or where to find trace/CSV performance results.
---

# Crossbench Benchmark Runner

Use this skill when a developer asks for help running a Crossbench benchmark or
finding its results.

### 1. Determining the Runner Type

Crossbench tests in Chromium use one of two runners: **Native Benchmarks**
(`cb.py`) or **Web-Test CUJs** (`run.py`). Use the inventory and rules below to
identify the runner immediately without searching the repository.

#### A. Known Test Inventory

- **Native Benchmarks (`third_party/crossbench/cb.py`)**: Standard synthetic
  press suites, microbenchmarks, and core page-load/startup harnesses:
  - *Press & Synthetic*: `speedometer` (`speedometer_3.1`, `speedometer_3.0`,
    `speedometer_2.1`, `speedometer_main`), `jetstream` (`jetstream_3.0`,
    `jetstream_2.2`, `jetstream_main`), `motionmark` (`motionmark_1.4`,
    `motionmark_1.3`), `webxprt5`
  - *Startup & Page Load*: `browser-startup`, `loadline2-phone`,
    `loadline2-tablet`, `loadline-phone`, `loadline-tablet`, `loading`
  - *Resource, AI & Other*: `memory`, `powerline`, `web-power`, `blink-ai`,
    `webai`, `embedder`, `devtools-frontend`, `manual`
- **Web-Test CUJs**
  (`third_party/crossbench-web-tests/cuj/crossbench/runner/run.py`): Multi-step
  Critical User Journeys (CUJs) and interactive app/site workflows driven by
  `.page-config.hjson` action blocks:
  - *Google Workspace*: `docs`, `docs-tab-switches`, `sheets`, `slides`,
    `gmail`, `classroom`, `drive-cleanup`
  - *Conferencing & RTC*: `meet`, `meet-note`, `heavy-meet-note`,
    `meet-share-note`, `local-conference`, `videocall`, `rtc-peer-connection`
  - *Media & Social*: `youtube`, `youtube-watch-browse`, `tiktok`, `video-drm`,
    `video-drm-seek`, `video-seek`, `video-playback-*`
  - *Browsing & Stress*: `browsing`, `browsing-multitab`,
    `browsing-multiwindow`, `simultaneous-load`, `open-close`, `tab-stress`,
    `cpu-stress`, `memory-pressure`, `gpu-swap`, `click-input-latency`,
    `typing-input-latency`, `page-click`, `page-scroll`, `internet-speed-test`

#### B. Disambiguation & Naming Rules

- **Overlapping Names (Avoid Naive Repo Search)**: Several native benchmarks
  (`browser-startup`, `speedometer_*`, `jetstream_*`, `motionmark_*`,
  `loadline*`, `memory`, `web-power`, `webxprt5`) also have wrapper directories
  under `third_party/crossbench-web-tests/`. **Always default to Native
  (`cb.py`)** for these tests unless the user explicitly mentions `web-tests`,
  `CUJ`, `run.py`, `--variants`, or `--secrets`.
- **Name Normalization**: Users often reference Chromium bot config names or
  underscores (e.g., `browser_startup` or `browser_startup.crossbench` from
  `bot_platforms.py`). Map these to the canonical Crossbench CLI name (e.g.,
  `browser-startup` for `cb.py`).
- **CLI Discovery (For Unknown Tests or Variants)**: Instead of grepping the
  repo, query the runners directly to list valid names, stories, or variants:
  - List all Native Benchmarks and aliases:
    `vpython3 third_party/crossbench/cb.py describe benchmarks`
  - Inspect stories and options for a specific Native Benchmark:
    `vpython3 third_party/crossbench/cb.py describe benchmark <name>`
  - List all Web-Test CUJs and their available `--variants`:
    ```bash
    vpython3 third_party/crossbench-web-tests/cuj/crossbench/runner/run.py \
      --list
    ```

#### C. Runner Commands

**Native Benchmarks (e.g., `browser-startup`, `speedometer_3.1`)**

- **Path**: `third_party/crossbench/cb.py`
- **Command**:
  ```bash
  vpython3 third_party/crossbench/cb.py <benchmark_name> \
    --browser=chrome \
    --repeat=1
  ```

**Web-Tests (e.g., `heavy-meet-note`, `docs`)**

- **Path**: `third_party/crossbench-web-tests/cuj/crossbench/runner/run.py`
- **Command**:
  ```bash
  cd third_party/crossbench-web-tests/cuj/crossbench/runner/
  vpython3 run.py --platform local --tests <test_name> \
    --variants <variant_name>
  ```

### 2. Troubleshooting Local Execution (CloudTop / Linux Desktop)

If the developer encounters crashes running locally, check for these common
environmental limitations:

- **No Display / Headless Crash**: If the browser exits immediately
  (`Could not start WebDriver: session not created: Chrome instance exited`),
  they are likely on a headless VM. Tell them to prepend `xvfb-run` to their
  command or use `--browser=chrome-headless`.
- **Unsupported `downloads` Probe**: The `downloads` probe is only supported on
  Android and ChromeOS. If running on local Linux/Mac, it will throw
  `NotImplementedError: Probe(DownloadsProbe): Unsupported browser`. Advise them
  to use a custom `probe-config.hjson` that omits the `downloads` probe (or
  override the template inheritance).
- **Hanging `screenshot` Probe**: If the test hangs indefinitely on a
  VM/CloudTop, the `screenshot` probe is likely waiting for `gnome-screenshot`
  (which requires a physical GNOME desktop). Advise them to remove the
  `screenshot` probe from their config.

### 3. Analyzing the Results

When the developer asks where to find the data after a successful run, guide
them to the `results/` directory generated at the end of the execution.

**Aggregated Results (High Level)**

- Located at `results/latest/cb.results.json`.
- This file contains merged statistical summaries (min, max, average).
- *Warning*: The generic `trace_processor` probe strips story metadata
  (`cb_story`) during JSON aggregation. Do not rely on this file if you need
  metrics separated by individual stories (e.g., `blank` vs. `newtab`).

**Unmerged Metrics (By Story)**

- To see exact timings separated by story, look at the raw Trace Processor CSVs.
- **Path**: `results/latest/output/trace_processor/<metric_name>.csv`.
- This file retains the `cb_story` column, allowing developers to see exact
  latencies for each scenario.

**Perfetto Traces (For UI Inspection)**

- The raw `.pb.gz` traces are saved per-browser and per-story.
- **Path**:
  `results/latest/<browser_dir>/stories/<story_name>/0/perfetto.trace.pb.gz`.
- Advise the developer to download this file and open it in
  `https://ui.perfetto.dev/` to debug trace events manually.

### 4. Advanced Configuration Tips

- **Multi-Arm Comparisons**: Developers can define multiple browsers/variants in
  a `browser.config.hjson` file and pass it via `--browser-config`. Crossbench
  natively interleaves the runs and outputs a comparative JSON report. Note:
  `--browser` and `--browser-config` are mutually exclusive in the CLI.
- **Instant Events in SQL**: When writing `trace_processor` SQL queries for
  Instant Events (which have a duration of 0 in Perfetto), remind the developer
  to query the delta between timestamps (`paint.ts - start.ts`) rather than
  querying the `dur` column.

### 5. Chromium Infrastructure Integration (Shard Maps)

When a Crossbench benchmark is added or modified in Chromium's
`tools/perf/core/bot_platforms.py` or its `schedule` CSV files, the changes will
not reach the swarming bots automatically.

- **Required Action**: You must regenerate the JSON shard maps by running
  `vpython3 tools/perf/core/generate_perf_sharding.py update -o` from the
  Chromium root. The resulting JSON changes in `tools/perf/core/shard_maps/`
  must be included in the CL.

### 6. Pinpoint & Trybot Executions

- **Restricted Bots**: Some performance bots (e.g., `mac-m1-pro-perf`) are
  CI-only and will return a "No props found" or "Bot invalid" error if used for
  on-demand Pinpoint jobs. Advise users to test on equivalent capacity bots like
  `mac-m1_mini_2020-perf` or standard trybots like `linux-perf-rel`.

### 7. Common Trace Processor Gotchas

- **Missing Trace Events (Empty CSVs)**: If a `trace_processor` SQL query
  returns empty results, the event was likely dropped by Perfetto. Verify that
  the event's category (e.g., `startup`) is explicitly listed in the
  `enabled_categories` of the `perfetto` probe config.
- **Dashboard Story Merging**: The Chromium `crossbench_result_converter.py`
  script ingests the aggregated `cb.results.json` file. Because Crossbench's
  `MetricsMerger` strips `cb_story` metadata for generic `trace_processor`
  queries, multiple stories will collapse into a single "Default" bucket on the
  Skia Perf dashboard. To separate them, either split the tests into separate
  benchmark registrations in `bot_platforms.py` or write a custom converter in
  `chromium/src`.
