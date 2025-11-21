Contains scripts to benchmark chrome builds.

There are a variety of different benchmarks you could run, but:
* `run_action.py` allows you to benchmark a particular build action, across a
  variety of configurations.
* `compare_autoninja.py` runs the same autoninja command on several output
  directories, and generates a database containing performance metrics for each
  invocation
