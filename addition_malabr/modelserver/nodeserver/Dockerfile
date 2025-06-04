# Use a Debian-based Node.js image (glibc is available here)
FROM node:18

# Create and set the working directory
WORKDIR /app

# Copy package files first for caching
COPY package*.json ./

# Install dependencies
RUN npm install

# Copy the rest of the application code
COPY . .

# Expose the port that the app listens on
EXPOSE 5000

# Start the server
CMD ["node", "server.js"]

