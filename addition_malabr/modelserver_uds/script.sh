#!/bin/bash
mkdir -p /tmp/shared-sockets  # Shared socket location

# build

# podman build -t unix-socket-server .

# --rm
# Remove the container automatically after it stops.

# -d
# Detached mode — run container in background.

# -v /tmp:/sockets:Z	
# Mount shared directory with SELinux relabeling

podman run --rm -d \
  --name unix-server \
  -v "/tmp/shared-sockets:/sockets:Z" \
  unix-socket-server

