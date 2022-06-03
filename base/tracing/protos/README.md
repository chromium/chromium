# Perfetto typed events for Chrome

**NOTE**: This is a work-in-progress.

In order to simplify adding new typed events for Chrome tracing, a protobuf extension support is
currently being implemented in Perfetto. The plan is that this folder is going to contain Chrome's
extensions to TrackEvent, and the directory is going to be autorolled into Perfetto repository.

More information: https://perfetto.dev/docs/design-docs/extensions

As this is developed, the current process to add new types of trace events is documented on
go/chrometto.
