# Review RAG Indexer

This code is responsible for creating indexes for the Review RAG service. If
a compatible index exists from a previous run, it will be incrementally built
upon. Otherwise, a fresh index will be created.

## Process Overview

The following steps are the general blocks of work that the script performs in
order to create an index.

<!-- TODO(b/517156708): Add additional sections as the script is completed. -->

### Determine Run Type

The script looks for the previous run's manifest information in CIPD. If the
manifest does not exist (indicating no previous runs have succeeded) or the
script determines that it is no longer relevant (e.g. the window over which
the index is created does not match), then the run will be marked as a clobber
run with a fresh index created. Otherwise, only new data since the last run
will be processed.

### Process Local Git Data

All git commits in the determined window are examined and the following actions
are performed:

  1. Relevant information from the CL is pulled and stored in a more
     script-compatible format such as CL description, revision, and commit
     position.
  2. The state of `DIR_METADATA` and related files are reconstructed for the
     first commit and incrementally updated when a commit changes relevant
     files. The correct `DIR_METADATA` state is then associated with each local
     commit that gets processed.
