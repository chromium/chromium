# Use Ubuntu as base image
FROM ubuntu:22.04

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install system dependencies
RUN apt-get update && apt-get install -y \
    python3 \
    python3-pip \
    python3-dev \
    build-essential \
    git \
    curl \
    sudo && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy your Flask app code
COPY modelserver/server.py .
COPY modelserver/myrequirements.txt .

# Install Python packages
RUN pip install -r myrequirements.txt

# Expose the Flask app port
EXPOSE 5000

# Run the Python Flask app
CMD ["python3", "server.py"]
