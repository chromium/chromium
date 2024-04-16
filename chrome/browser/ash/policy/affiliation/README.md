chrome/browser/ash/policy/affiliation
==========================================

This directory should contain code related to affiliation, a mechanism
that determines if user and device are managed by the same organization.

Managed users and managed devices can have affiliation IDs that were
set by their admin. When the device and the logged in user are both
managed and have a matching affiliation ID, they are said to be
affiliated. This influences the behavior of some policies that only
apply in the affiliated or the unaffiliated case.

TODO(crbug.com/40185259): Add more information about affiliation.
