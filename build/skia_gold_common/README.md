This directory contains Python code used for interacting with the Skia Gold
image diff service. It is used by multiple test harnesses, e.g.
`//build/android/test_runner.py` and
`//content/test/gpu/run_gpu_integration_test.py`. A place such as
`//testing/` would likely be a better location, but causes issues with
V8 since it imports `//build/` but not all of Chromium src.
