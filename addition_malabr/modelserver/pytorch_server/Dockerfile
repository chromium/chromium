# Use an official Python 3.10 slim image (glibc-based)
FROM python:3.10-slim

# Set the working directory inside the container
WORKDIR /app

# Copy the requirements file first (to leverage Docker cache)
COPY requirements.txt .

# Install system dependencies (if needed, e.g. build tools)
RUN apt-get update && apt-get install -y build-essential && apt-get clean

# Install the Python dependencies
RUN pip install --no-cache-dir -r requirements.txt

# Copy the rest of the application code into the container
COPY . .

# Expose the port the server will run on
EXPOSE 5000

# Run the server using hypercorn with uvloop
CMD ["hypercorn", "server:app", "--bind", "0.0.0.0:5000", "--worker-class", "uvloop", "--log-level", "debug"]

