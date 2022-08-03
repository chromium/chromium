This directory contains code that reports events and telemetry through the
Encrypted Reporting Pipeline (ERP).

Generally speaking, new events that are reported through the ERP should be added
here. Exceptions include events that may be more conveniently enqueued from
another ChromiumOS process (which are usually best implemented within that
process), and other exceptional circumstances.

Telemetry is data that are collected once or periodically. They are reported via
protos in `components/reporting/proto/synced/metric_data.proto`.
