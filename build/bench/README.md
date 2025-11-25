Contains scripts to benchmark chrome builds.

There are a variety of different benchmarks you could run, but:
* `run_action.py` allows you to benchmark a particular build action, across a
  variety of configurations.
* `compare_autoninja.py` runs the same autoninja command on several output
  directories, and generates a database containing performance metrics for each
  invocation
* `ftime.py` analyses the output of `-ftime-trace` for a build to examine where
  our time was spent, on a micro level, but for a whole build. In particular, it
  currently analyses the amount of time spent `#include`ing each file.
