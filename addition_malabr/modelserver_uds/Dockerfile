
 # Use official Python image
FROM python:3.11-slim

# Create a directory for server code and socket
WORKDIR /app

# Copy server script
COPY server.py .

# Create the mount point for the socket
RUN mkdir -p /sockets

# Expose nothing (UNIX socket only)

# Run the server
CMD ["python", "-u", "server.py"]
