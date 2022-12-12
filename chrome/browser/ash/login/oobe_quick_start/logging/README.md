This directory implements several logging macros to be used with Quick Start.

Use `QS_LOG(severity)` for general-purpose logging, and will emit logs to the
standard logging system. VERBOSE messages logged in this manner can be emitted
to the logs by using the `--quick-start-verbose-logging` command-line flag.

See go/cros-quickstart-logging for more info.
