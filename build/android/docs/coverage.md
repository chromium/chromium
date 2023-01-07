# Android code coverage instructions

These are instructions for collecting code coverage data for android
instrumentation and JUnit tests. For Clang(C++) code coverage refer to [clang coverage].

[TOC]

## How JaCoCo coverage works

In order to use JaCoCo code coverage, we need to create build time pre-instrumented
class files and runtime **.exec** files. Then we need to process them using the
[build/android/generate_jacoco_report.py](https://source.chromium.org/chromium/chromium/src/+/main:build/android/generate_jacoco_report.py) script.

## How to collect coverage data

1. Use the following GN build arguments:

  ```gn
  target_os = "android"
  use_jacoco_coverage = true
  ```

   Now when building, pre-instrumented files will be created in the build directory.

2. Run tests, with option `--coverage-dir <directory>`, to specify where to save
   the .exec file. For example, you can run chrome JUnit tests:
   `out/Debug/bin/run_chrome_junit_tests --coverage-dir /tmp/coverage`.

3. The coverage results of JUnit and instrumentation tests will be merged
   automatically if they are in the same directory.

## How to generate coverage report

1. Now we have generated .exec files already. We can create a JaCoCo HTML/XML/CSV
   report using `generate_jacoco_report.py`, for example:

  ```shell
  build/android/generate_jacoco_report.py \
     --format html \
     --output-dir /tmp/coverage_report/ \
     --coverage-dir /tmp/coverage/ \
     --sources-json-dir out/Debug/ \
  ```
   Then an index.html containing coverage info will be created in output directory:

  ```
  [INFO] Loading execution data file /tmp/coverage/testTitle.exec.
  [INFO] Loading execution data file /tmp/coverage/testSelected.exec.
  [INFO] Loading execution data file /tmp/coverage/testClickToSelect.exec.
  [INFO] Loading execution data file /tmp/coverage/testClickToClose.exec.
  [INFO] Loading execution data file /tmp/coverage/testThumbnail.exec.
  [INFO] Analyzing 58 classes.
  ```

2. For XML and CSV reports, we need to specify `--output-file` instead of `--output-dir` since
   only one file will be generated as XML or CSV report.
  ```shell
  build/android/generate_jacoco_report.py \
    --format xml \
    --output-file /tmp/coverage_report/report.xml \
    --coverage-dir /tmp/coverage/ \
    --sources-json-dir out/Debug/ \
  ```

   or

  ```shell
  build/android/generate_jacoco_report.py \
    --format csv \
    --output-file /tmp/coverage_report/report.csv \
    --coverage-dir /tmp/coverage/ \
    --sources-json-dir out/Debug/ \
  ```
3. If generating coverage and there are duplicate class files, as can happen
   when generating coverage for downstream targets, use the
   `--include-substr-filter` option to choose jars in the desired directory. Eg.
   for generating coverage report for Clank internal repo
  ```shell
  build/android/generate_jacoco_report.py --format html \
   --output-dir /tmp/coverage_report/ --coverage-dir /tmp/coverage/ \
   --sources-json-dir out/java_coverage/ \
   --include-substr-filter obj/clank
  ```

[clang coverage]: https://chromium.googlesource.com/chromium/src/+/HEAD/docs/testing/code_coverage.md