DEPRECATED. If you're about adding new service for ash-chrome only,
consider using //chromeos/ash/components.

This directory holds [services](/services) that are:
- Chrome OS UI (ash) specific.
- They run in their own utility processes, and cannot call ash code directly.
- Chrome can add a DEP on these services just for the sole purpose of launching them (until ash supports launching its own services).
