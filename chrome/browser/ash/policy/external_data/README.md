chrome/browser/ash/policy/external_data
============================================

This directory should contain code that handles policies which rely on external
data.

Each policy has one of three different formats:
* STRING, a free-form string value,
* JSON, a valid string in the .json format,
* EXTERNAL, an arbitrary file that has to be downloaded separately to the
normal policy retrieval process.

In the EXTERNAL case, the policy value is (internally) represented by a JSON
string that contains the URL of the external file, and a hash for verification.
The code in this directory is responsible for the retrieval, verification
and caching of the external data. Behavior that is specific to individual
policies is defined via handlers in the external_data/handlers/ subdirectory.
