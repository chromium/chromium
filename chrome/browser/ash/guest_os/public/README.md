# Guest OS Public

This folder contains the (in-development) public API for Guest OS. Here we
expose a common API over the different types of Guest OS (Crostini, Borealis,
Bruschetta, PluginVM, etc) so callers elsewhere in Chrome can support them
without being specialised for each specific Guest.

## Architecture

Guest OS Service is the parent that callers interact with. It's made up of
registries for individual features that Guest OSs provide (e.g. sharing files
from the guest to the host). Each client (e.g. the files app) can query the
registry to get a list of instances, one per guest, then these instances provide
the backend for the feature (e.g. mounting, providing icons).

TODO(davidmunro): Actual docs, diagrams, etc, once the design is settled.
